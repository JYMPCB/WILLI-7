#include <Arduino.h>
#include "app.h"
#include "app_globals.h"
#include "../tasks/tasks.h"
#include "../ui_glue/ui_refresh.h"
#include "../sensor/hall_sensor.h"
#include "../training/training_interval.h"
#include "rgb/rgb_mgr.h"
#include "../rgb/rgb_gpio.h"
#include "service/service_mgr.h"

//#include "beep/beep_mgr.h"
#define ESP70C
#ifdef ESP70C
  #include "../gui/gui.h"
  #include "../ui/ui.h"
#endif


void app_init()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  esp_log_level_set("wifi", ESP_LOG_VERBOSE);
  esp_log_level_set("esp_netif", ESP_LOG_VERBOSE);
  esp_log_level_set("tcpip_adapter", ESP_LOG_VERBOSE);

    // (opcional) logs de memoria...
    log_i("Board: %s", BOARD_NAME);
    log_i("CPU: %s rev %d, CPU Freq: %d Mhz, %d core(s)", ESP.getChipModel(), ESP.getChipRevision(), getCpuFrequencyMhz(), ESP.getChipCores());
    log_i("Free heap: %d bytes", ESP.getFreeHeap());
    log_i("Free PSRAM: %d bytes", ESP.getPsramSize());
    log_i("SDK version: %s", ESP.getSdkVersion());      

    if (psramFound()) {
        Serial.println("✅ PSRAM detectada y funcional");
        Serial.print("Tamaño total PSRAM: ");
        Serial.print(esp_spiram_get_size() / 1024);
        Serial.println(" KB");
    } else {
        Serial.println("❌ PSRAM NO detectada");
    }
    // Mostrar cantidad de memoria libre
    Serial.print("Heap interno libre: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Heap PSRAM libre: ");
    Serial.println(ESP.getFreePsram());

    // init display
    #ifdef ESP70C
    gui_start();
    #else
    smartdisplay_init();
    __attribute__((unused)) auto disp = lv_disp_get_default();
    lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    ui_init();
    #endif

    // mutex UI
    g_ui_mutex = xSemaphoreCreateMutex();

    // refresh UI LVGL
    lv_timer_create(ui_refresh_cb, 200, NULL);

    // tasks
    startTasks();

    //inicia intervalado
    ti_init();

    //inicia RGB
    rgb_gpio_init_active_low();

    //inicia tono
    //beep_init(17);
    //beep_once(1500, 100);   // equivalente a tone(17,1500,100) pero no bloqueante

    //inicia service
    g_service.begin();

    
}
