#include <Arduino.h>
#include "hall_sensor.h"
#include "esp_cpu.h"
#include "freertos/portmacro.h"
#include "../app/app_globals.h"

//extern void IRAM_ATTR sensor(); // o definila acá directamente

void IRAM_ATTR sensor()
{
  uint32_t now = esp_cpu_get_ccount();
  uint32_t dt  = now - g_last_cycles;

  if(dt > g_min_cycles) {
    g_last_cycles = now;
    g_period_cycles = dt;
    g_giro++;
  }
}

void hall_sensor_init(int sensorHall)
{
  pinMode(sensorHall, INPUT_PULLUP);
  g_cpu_mhz = getCpuFrequencyMhz();
  g_last_cycles = esp_cpu_get_ccount();
  g_min_cycles = (uint32_t)(10UL * 1000UL * g_cpu_mhz); // 10ms

  attachInterrupt(digitalPinToInterrupt(sensorHall), sensor, RISING);
}

void hall_sensor_reset()
{
  portENTER_CRITICAL(&g_isr_mux);
  g_giro = 0;
  g_period_cycles = 0;
  g_last_cycles = esp_cpu_get_ccount();
  portEXIT_CRITICAL(&g_isr_mux);
}

void hall_sensor_snapshot(uint32_t *giro, uint32_t *period_cycles)
{
  portENTER_CRITICAL(&g_isr_mux);
  *giro = g_giro;
  *period_cycles = g_period_cycles;
  portEXIT_CRITICAL(&g_isr_mux);
}

void hall_sensor_enable(bool isrChange, int sensorHall){
  if(isrChange) {
      attachInterrupt(digitalPinToInterrupt(sensorHall), sensor, RISING);
    } else {      
      detachInterrupt(digitalPinToInterrupt(sensorHall));
    }
}