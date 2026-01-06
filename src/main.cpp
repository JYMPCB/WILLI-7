#include <Arduino.h>

#define ESP70C 
#ifdef ESP70C
#include <lv_conf.h>
#include <lvgl.h>
#include "gui/gui.h"
#include "ui/ui.h"
#endif
#ifdef ESP35C
#include <esp32_smartdisplay.h>
#include <ui/ui.h>
#endif

#include "app/app.h"
#include "ui_state.h"
#include "sensor/hall_sensor.h"
#include "app/app_globals.h"   
#include "wifi_mgr/wifi_mgr.h"
//#include "beep/beep_mgr.h"

#include <WiFi.h>
#include "dataWilli.h"      
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

//NVS para almacenar datos no volatiles
#include <Preferences.h>
Preferences prefs;

//DEFINICION DE HARDWARE                                                               
#ifdef ESP70C
#define beep  17
#define sensorHall 18
//I2S sonido
/*#define I2S_DOUT  17
#define I2S_BCLK  0
#define I2S_LRC   18*/
#endif
#ifdef ESP35C
#define sensorHall 22
#define beep 26
#endif

volatile uint32_t g_gui_worstGap_ms = 0;
volatile uint32_t g_gui_last_lv_us  = 0;
volatile uint32_t g_gui_stack_hw    = 0;

//TAREAS
void guiTask(void *pv)
{
  for(;;){
    // Devuelve ms hasta el próximo timer que LVGL necesita atender
    uint32_t wait_ms = lv_timer_handler();

    // Clamp para que nunca se vaya al pasto (evita backlog gigante)
    if(wait_ms > 20) wait_ms = 20;   // 20ms (~50 Hz)
    if(wait_ms < 1)  wait_ms = 1;

    vTaskDelay(pdMS_TO_TICKS(wait_ms));
  }
}

void logTask(void *){
  for(;;){
    Serial.printf("[gui] worstGap=%lu ms lv=%lu us stackHW=%lu words\n",
      (unsigned long)g_gui_worstGap_ms,
      (unsigned long)g_gui_last_lv_us,
      (unsigned long)g_gui_stack_hw
    );
    g_gui_worstGap_ms = 0;

    /*Serial.printf("[flush] worst=%lu us last=%lu us\n",
    (unsigned long)g_flush_worst_us, (unsigned long)g_flush_last_us);
    g_flush_worst_us = 0;*/

    Serial.printf("[flush] count/s=%lu worst=%lu us worstArea=%ldx%ld\n",
    (unsigned long)g_flush_count,
    (unsigned long)g_flush_worst_us,
    (long)g_flush_worst_w, (long)g_flush_worst_h
    );
    g_flush_count = 0;
    g_flush_worst_us = 0;

    Serial.printf("[touch] count/s=%lu worst=%lu us last=%lu us\n",
    (unsigned long)g_touch_count,
    (unsigned long)g_touch_worst_us,
    (unsigned long)g_touch_last_us
    );
    g_touch_count = 0;
    g_touch_worst_us = 0;

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void setup()
{ 
  app_init();

  //inicia sensores
  hall_sensor_init(sensorHall);
  //xTaskCreatePinnedToCore(logTask, "log", 4096, NULL, 1, NULL, 0);  //TAREA PARA PRUEBAS 
  
}

void loop()
{
  // --- Manejo robusto de interrupción por transición de pantalla ---
  static bool lastCfg = false;

  if(habConfig && !lastCfg) {
    hall_sensor_enable(false, sensorHall);
  }
  if(!habConfig && lastCfg) {
    hall_sensor_enable(true, sensorHall);
  }
  lastCfg = habConfig;
  wifi_mgr_loop();
  //beep_update();
  delay(5); // cede CPU, evita loop apretado
}





