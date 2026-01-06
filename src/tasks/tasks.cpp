#include <Arduino.h>
#include "tasks.h"
#include "app/app_globals.h"
#include "rgb/rgb_gpio.h"
#include "training/training_interval.h"

void mathTask(void *pv)
{
 
  extern volatile bool g_workout_frozen;

  static bool startedOnce = false;

  const uint32_t tiempoVelMin = 2; // segundos sin giro para considerar stop

  uint32_t last_giro = 0;
  uint32_t counterNoGiro = 0;

  uint32_t last_ms = millis();
  uint32_t acc_ms  = 0;

  TickType_t last = xTaskGetTickCount();

  for(;;) {

    if(g_workout_frozen) {
    vTaskDelay(pdMS_TO_TICKS(50));  // o 100ms
    continue;                       // NO recalcular, pero la task sigue viva
    }    

    //vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    // reset pedido desde UI
    if(g_reset_req) {
      g_reset_req = false;

      float espacio_now = espacio;
      sessionActive = false;
      startedOnce = false;

      tiempoHour = tiempoMin = tiempoSec = 0;
      espacio = 0.0f;

      acc_ms = 0;
      last_ms = millis();

      portENTER_CRITICAL(&g_isr_mux);
      g_giro = 0;
      g_period_cycles = 0;
      g_last_cycles = esp_cpu_get_ccount();
      portEXIT_CRITICAL(&g_isr_mux);

      g_speed_reset_req = true;

      // ocultar cartel pausa
      xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
      g_ui.pausaVisible = false;
      xSemaphoreGive(g_ui_mutex);

      // mantener última config: volver a ARMED
      if(g_train_mode == TRAIN_INTERVAL) ti_rearm(espacio_now);
      else                              ti_stop();
    }

    ui_state_t out;
    xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
    out = g_ui;                 // ✅ partir del estado actual (incluye speed/pace/cursor)
    xSemaphoreGive(g_ui_mutex);

    uint32_t giro_now;
    portENTER_CRITICAL(&g_isr_mux);
    giro_now = g_giro;
    portEXIT_CRITICAL(&g_isr_mux);

    bool hubo_giro = (giro_now != last_giro);
    if(hubo_giro) {
      counterNoGiro = 0;
      last_giro = giro_now;
    } else {
      counterNoGiro++;
    }

    // inicio inteligente
    if(!g_workout_frozen && !startedOnce && giro_now >= 3) {
      startedOnce = true;
      sessionActive = true;

      if(g_train_mode == TRAIN_INTERVAL) {
        ti_on_session_started(espacio);
      }
    }

    bool movimiento = (counterNoGiro <= tiempoVelMin);

    // Si terminó y está congelado, barras visibles y controles accesibles
    bool force_controls = g_workout_frozen; 
      
    out.headerHidden   = movimiento && !force_controls;
    out.settingsHidden = movimiento && !force_controls;
    out.showNetTime    = (!movimiento) || force_controls;
    // Mostrar reinicio si hay sesión activa O si quedó congelado el final
    out.showReinitBtn  = sessionActive || force_controls;

    // ---- tiempo ----
    uint32_t now_ms = millis();
    uint32_t dt_ms  = now_ms - last_ms;
    last_ms = now_ms;

    if(sessionActive && movimiento) {
      acc_ms += dt_ms;
      while(acc_ms >= 1000) {
        acc_ms -= 1000;
        tiempoSec++;
        if(tiempoSec >= 60) { tiempoSec = 0; tiempoMin++; }
        if(tiempoMin >= 60) { tiempoMin = 0; tiempoHour++; }
      }
    }

    if(g_train_mode != TRAIN_INTERVAL || g_interval_is_dist) {
      snprintf(out.trainTimeStr, sizeof(out.trainTimeStr),
              "%02u:%02u:%02u",
              (unsigned)tiempoHour,
              (unsigned)tiempoMin,
              (unsigned)tiempoSec);
    }
    
    //---rest api ------
    uint32_t elapsed = (uint32_t)tiempoHour * 3600u + (uint32_t)tiempoMin  * 60u + (uint32_t)tiempoSec;
    out.elapsed_s = elapsed; //---valores hacia rest api ------

    // ---- distancia ----
    float espacio_m = drdisco * giro_now;
    
    // ✅ esto es CLAVE para intervalado (usa la global)
    espacio = espacio_m;

    out.dist_m = espacio_m; //---valores hacia rest api ------

    if(espacio_m < 1000) {
      snprintf(out.distStr, sizeof(out.distStr), "%.0f", espacio_m);
      strcpy(out.distUnit, "mt");
    } else {
      snprintf(out.distStr, sizeof(out.distStr), "%.3f", espacio_m * 0.001f);
      strcpy(out.distUnit, "km");
    }

    // ---- calorías ----
    float v_now;
    xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
    v_now = g_ui.velocidad;
    xSemaphoreGive(g_ui_mutex);

    float factor = 0.65f + 0.015f * logf(v_now + 1.0f);
    float calorias = peso * (espacio_m * 0.001f) * factor;
    snprintf(out.calStr, sizeof(out.calStr), "%.1f", calorias);
    g_kcal_total_live = calorias;    
    out.kcal = calorias; //---valores hacia rest api ------
    // ---- hora net ----
    if(out.showNetTime && habLocalTime) {
      struct tm dt;
      if(getLocalTime(&dt, 10)) {
        snprintf(out.timeStr, sizeof(out.timeStr), "%02d:%02d",
                 dt.tm_hour, dt.tm_min);
        snprintf(out.dateStr, sizeof(out.dateStr), "%02d/%02d/%04d",
                 dt.tm_mday, dt.tm_mon+1, dt.tm_year+1900);
      }
    }

    // Intervalado: update 1s
    if(g_train_mode == TRAIN_INTERVAL) {
      ti_update(1000, espacio);
    }

    // ✅ mezclar overlay del intervalado sobre OUT (pausa + restante por tiempo)
    ti_apply_overlay_to_ui(&out);

    out.session_active = sessionActive;     //---valores hacia rest api ------
    out.workout_frozen = g_workout_frozen;  //---valores hacia rest api ------

    // publicar snapshot
    xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
    g_ui = out;
    xSemaphoreGive(g_ui_mutex);
  }
}

void speedTask(void *pv)
{
  extern volatile bool g_workout_frozen;

  const TickType_t period = pdMS_TO_TICKS(100);
  TickType_t last = xTaskGetTickCount();

  const float v_max = 40.0f;

  // ---- umbral físico: período a 40 km/h según drdisco (m) ----
  // drdisco = 0.80384 m -> t40_us ≈ 72345 us
  const uint32_t t40_us = (uint32_t)((drdisco / (v_max / 3.6f)) * 1e6f + 0.5f);

  // Margen duro: rechaza pulsos más rápidos que ~40km/h (con tolerancia)
  const float margin = 0.9f;                 // 0.85..0.90 (más alto = más duro)
  const uint32_t t_min_us = (uint32_t)(t40_us * margin);

  // ---- filtro duro de picos ----
  static float v_hold = 0.0f;                 // último valor creíble
  static uint8_t spike_cnt = 0;
  const float jump_allow = 5.0f;              // km/h por tick (100ms)
  const uint8_t need_confirm = 2;             // lecturas seguidas para aceptar salto grande

  // opcional: si falta un pulso, mantenemos el último período válido por 1 tick
  static uint32_t per_us_last_valid = 0;

  for(;;) {

    if(g_workout_frozen) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    vTaskDelayUntil(&last, period);

    // reset pedido desde mathTask
    if(g_speed_reset_req) {
      g_speed_reset_req = false;
      v_hold = 0.0f;
      spike_cnt = 0;
      per_us_last_valid = 0;
      
      xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
      g_ui.velocidad = 0;
      g_ui.cursorAngle = 0;
      strcpy(g_ui.speedStr, "0.0");
      strcpy(g_ui.ritmoStr, "00'00\"");
      xSemaphoreGive(g_ui_mutex);
      continue;
    }

    uint32_t per_cycles;
    portENTER_CRITICAL(&g_isr_mux);
    per_cycles = g_period_cycles;
    portEXIT_CRITICAL(&g_isr_mux);

    uint32_t per_us = (per_cycles > 0) ? (per_cycles / g_cpu_mhz) : 0;

    //bool new_sample = (per_cycles > 0);
    // ---- filtro físico: descartar pulsos demasiado rápidos (glitch/rebote) ----
    if(per_us > 0 && per_us < t_min_us) {
      per_us = 0;
    }

    // opcional: si viene 0 un tick, usar el último válido para evitar jitter
    if(per_us == 0 && per_us_last_valid != 0) {
      per_us = per_us_last_valid;
    }

    float velocidad = 0.0f;
    if(per_us > 0) {
      per_us_last_valid = per_us;

      float t = per_us * 1e-6f;
      velocidad = (drdisco / t) * 3.6f;

      if(velocidad < 0.1f) velocidad = 0.0f;
      if(velocidad > v_max) velocidad = v_max;
    }

    // ---- confirmación de picos (duro) ----
    if(!sessionActive) {
      velocidad = 0.0f;
      v_hold = 0.0f;
      spike_cnt = 0;
    } else {      
      if(velocidad > (v_hold + jump_allow)) {
        spike_cnt++;
        if(spike_cnt < need_confirm) {
          velocidad = v_hold;   // ignorar pico aislado
        } else {
          v_hold = velocidad;   // aceptado si se sostuvo
          spike_cnt = 0;
        }
      } else {
        spike_cnt = 0;
        v_hold = velocidad;
      }
      
    }

    char speedStr[16];
    snprintf(speedStr, sizeof(speedStr), "%.1f", (double)velocidad);

    char ritmoStr[16];
    uint8_t r = 0, g = 0, b = 0;

    if(velocidad > 0.1f) {
      float ritmo = 60.0f / velocidad;
      if(ritmo > 30) ritmo = 30;

      pace_to_rgb(ritmo, r, g, b);

      int rm = (int)ritmo;
      int rs = (int)((ritmo - rm) * 60.0f + 0.5f);
      if(rs >= 60) { rs = 0; rm++; }

      snprintf(ritmoStr, sizeof(ritmoStr), "%02d'%02d\"", rm, rs);

      if(sessionActive) {
        rgb_set_target_u8(r, g, b);
      } else {
        r = g = b = 0;
      }
    } else {
      strcpy(ritmoStr, "00'00\"");
      r = g = b = 0;
    }    

    int angle = (int)((sessionActive ? v_hold : 0.0f) * 45.0f);
    if(angle < 0) angle = 0;
    if(angle > (int)(v_max * 45.0f)) angle = (int)(v_max * 45.0f);

    xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
    g_ui.velocidad = velocidad;
    g_ui.cursorAngle = angle;
    strlcpy(g_ui.speedStr, speedStr, sizeof(g_ui.speedStr));
    strlcpy(g_ui.ritmoStr, ritmoStr, sizeof(g_ui.ritmoStr));
    xSemaphoreGive(g_ui_mutex);
  }
}


void startTasks()
{
  xTaskCreatePinnedToCore(guiTask,   "gui",   8192, NULL, 4, NULL,            1);
  xTaskCreatePinnedToCore(mathTask,  "math",  4096, NULL, 3, &g_mathTaskHandle,  0);
  xTaskCreatePinnedToCore(speedTask, "speed", 4096, NULL, 3, &g_speedTaskHandle, 0);
}

