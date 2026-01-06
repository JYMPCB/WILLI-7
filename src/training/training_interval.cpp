#include "training_interval.h"
#include <string.h>
#include <stdio.h>
#include "ui_state.h"
#include "app/app_globals.h"
#include <math.h>

// ----------------- helpers -----------------
static void fmt_mmss(uint32_t ms, char out[8]) {
  uint32_t s = ms / 1000;
  uint32_t mm = s / 60;
  uint32_t ss = s % 60;
  snprintf(out, 8, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

// ----------------- runtime -----------------
static ti_cfg_t   g_cfg;
static ti_state_t g_state = TI_IDLE;

static uint8_t g_cur_series = 0;
static uint8_t g_cur_pass   = 0;

static uint32_t g_pass_remain_ms = 0;
static float    g_pass_remain_m  = 0.0f;
static uint32_t g_rest_remain_ms = 0;

static float g_last_espacio_m = 0.0f;

// ----------------- UI overlay (lo que la UI debe mostrar) -----------------
static bool g_ov_pausaVisible = false;
static bool g_ov_pausaIsSerie = false;
static char g_ov_pausaTitle[24] = "";
static char g_ov_pausaStr[8] = "";

static bool g_ov_timeOverride = false;   // si true, pisa trainTimeStr
static char g_ov_timeStr[16] = "";
static bool g_ov_distOverride = false;
static char g_ov_distStr[16] = "";

static char g_ov_seriesStr[8]   = "";    // series restantes
static char g_ov_passStr[8]     = "";    // pasadas restantes
static char g_ov_setpointStr[16]= "";    // setpoint (MM:SS o "XXX m")
// ----------------- contadores de ejecución -----------------
static uint8_t g_series_left = 0;   // series restantes (incluye la actual)
static uint8_t g_series_remaining = 0; // repeticiones de serie restantes (0 = última vuelta)
static uint8_t g_pass_left   = 0;   // pasadas restantes en la serie actual


//caché para guardar valores totales
static uint32_t g_total_work_ms = 0;
static float    g_total_dist_m  = 0.0f;
// si kcal la calculás en otra task, guardamos el “último total live”
static float    g_kcal_live_total = 0.0f;
// helper: si querés setear kcal desde afuera
void ti_set_kcal_total(float kcal_total) { g_kcal_live_total = kcal_total; }

static void overlay_update_cfg_strings(void)
{
  snprintf(g_ov_seriesStr, sizeof(g_ov_seriesStr), "%u", (unsigned)g_series_remaining);
  snprintf(g_ov_passStr,   sizeof(g_ov_passStr),   "%u", (unsigned)g_pass_left);

  if(g_cfg.mode == TI_PASS_BY_TIME) {
    char tmp[8];
    fmt_mmss(g_cfg.pass_time_ms, tmp);
    strlcpy(g_ov_setpointStr, tmp, sizeof(g_ov_setpointStr));
  } else {
    snprintf(g_ov_setpointStr, sizeof(g_ov_setpointStr),
             "%.0f", (double)g_cfg.pass_dist_m);
  }
}

// ----------------- internal helpers -----------------
static void overlay_clear(void) {
  g_ov_pausaVisible = false;
  g_ov_pausaIsSerie = false;
  g_ov_pausaTitle[0] = 0;
  g_ov_pausaStr[0] = 0;

  g_ov_timeOverride = false;
  g_ov_timeStr[0] = 0;
  g_ov_distOverride = false;
  g_ov_distStr[0] = 0;
}

static void overlay_set_pausa(bool visible, bool isSerie, const char *title, uint32_t remain_ms) {
  g_ov_pausaVisible = visible;
  if(visible) {
    g_ov_pausaIsSerie = isSerie;
    strlcpy(g_ov_pausaTitle, title, sizeof(g_ov_pausaTitle));
    fmt_mmss(remain_ms, g_ov_pausaStr);
  }
}

static void overlay_set_time_remain(uint32_t remain_ms) {
  g_ov_timeOverride = true;
  fmt_mmss(remain_ms, g_ov_timeStr);
}

static void overlay_set_dist_remain(float remain_m) {
  g_ov_distOverride = true;
  if(remain_m < 0) remain_m = 0;
  snprintf(g_ov_distStr, sizeof(g_ov_distStr), "%.0f", (double)remain_m);
}

// ----------------- transitions -----------------
static void enter_idle(void) {
  g_state = TI_IDLE;
  g_cur_series = 0;
  g_cur_pass = 0;
  g_pass_remain_ms = 0;
  g_pass_remain_m = 0.0f;
  g_rest_remain_ms = 0;
  overlay_clear();
}

static void enter_armed(void) {
  g_state = TI_ARMED;
  overlay_clear();
  overlay_update_cfg_strings(); 
}

static void enter_run(void) {
  g_state = TI_RUN;
  overlay_clear();

  if(g_cfg.mode == TI_PASS_BY_TIME) {
    g_pass_remain_ms = g_cfg.pass_time_ms;
    overlay_set_time_remain(g_pass_remain_ms);
  } else {
    g_pass_remain_m = g_cfg.pass_dist_m;
    overlay_set_dist_remain(g_pass_remain_m);
  }
}

static void enter_rest_pass(void) {
  g_state = TI_REST_PASS;
  g_rest_remain_ms = g_cfg.rest_pass_ms;
  overlay_clear();
  overlay_set_pausa(true, false, "PASADA EN PAUSA", g_rest_remain_ms);
}

static void enter_rest_series(void) {
  g_state = TI_REST_SERIES;
  g_rest_remain_ms = g_cfg.rest_series_ms;
  overlay_clear();
  overlay_set_pausa(true, true, "SERIE EN PAUSA", g_rest_remain_ms);
}

static void freeze_workout_final(void)
{
  g_workout_frozen = true;

  // ---- Totales planificados (B) ----
  uint32_t total_series = (uint32_t)g_cfg.num_series + 1;             // series extra + la actual
  uint32_t total_passes = (uint32_t)g_cfg.num_passes * total_series;  // pasadas totales

  if(g_cfg.mode == TI_PASS_BY_TIME) {
    // EXACTO: tiempo elegido * pasadas totales
    g_workout_final.total_time_ms = (uint32_t)(g_cfg.pass_time_ms * total_passes);

    // En modo tiempo: distancia real (depende del usuario)
    g_workout_final.total_dist_m  = g_total_dist_m;
  } else {
    // En modo distancia: distancia exacta por config
    g_workout_final.total_dist_m  = (float)total_passes * g_cfg.pass_dist_m;

    // Tiempo real (depende del ritmo)
    g_workout_final.total_time_ms = g_total_work_ms;
  }

  // ---- Promedios (con totales ya definidos) ----
  float hours = (g_workout_final.total_time_ms / 1000.0f) / 3600.0f;
  if(hours > 0.0f) g_workout_final.avg_speed_kmh = (g_workout_final.total_dist_m / 1000.0f) / hours;
  else g_workout_final.avg_speed_kmh = 0.0f;

  float km = g_workout_final.total_dist_m / 1000.0f;
  float minutes = (g_workout_final.total_time_ms / 1000.0f) / 60.0f;
  if(km > 0.0f) g_workout_final.avg_pace_min_km = minutes / km;
  else g_workout_final.avg_pace_min_km = 0.0f;

  // ---- Calorías finales (con velocidad promedio ya calculada) ----
  float v_avg = g_workout_final.avg_speed_kmh; // km/h
  float factor = 0.65f + 0.015f * logf(v_avg + 1.0f);
  g_workout_final.total_kcal = peso * (g_workout_final.total_dist_m * 0.001f) * factor;
}

static void enter_done(void) {
  g_state = TI_DONE;
  overlay_clear();
  freeze_workout_final();
  // opcional: mostrar 0/0
  g_series_left = 0;
  g_pass_left = 0;
  overlay_update_cfg_strings();
}

// ----------------- public API -----------------
void ti_init(void) {
  memset(&g_cfg, 0, sizeof(g_cfg));
  enter_idle();
}

void ti_apply_cfg(const ti_cfg_t *cfg) {
  if(!cfg) return;
  g_cfg = *cfg;
  //if(g_cfg.num_series == 0) g_cfg.num_series = 1;
  //if(g_cfg.num_passes == 0) g_cfg.num_passes = 1;
}

void ti_arm(void)
{
  g_total_work_ms = 0;
  g_total_dist_m  = 0.0f;
  // series_remaining = "cuántas series extra faltan" (puede ser 0)
  g_series_remaining = g_cfg.num_series;

  // pasadas de esta serie
  g_pass_left = g_cfg.num_passes;

  overlay_update_cfg_strings();   // que muestre series/pasadas/setpoint ya

  g_state = TI_ARMED;
}

void ti_rearm(float espacio_m_now)
{
  g_total_work_ms = 0;
  g_total_dist_m  = 0.0f;
  g_series_left = g_cfg.num_series;
  g_pass_left   = g_cfg.num_passes;

  g_last_espacio_m = espacio_m_now;

  overlay_update_cfg_strings();   // ✅ NUEVO

  g_state = TI_ARMED;
}

void ti_on_session_started(float espacio_m_now) {
  if(g_state != TI_ARMED) return;
  g_last_espacio_m = espacio_m_now;
  enter_run();
}

void ti_stop(void) {
  enter_idle();
}

ti_state_t ti_get_state(void) {
  return g_state;
}

static bool need_rest_pass(void)
{
  // Micropausa solo si hay más de 1 pasada
  return (g_pass_left > 0) && (g_cfg.rest_pass_ms > 0);
}

static bool need_rest_series(void) {
  return (g_series_remaining > 0);
}

void ti_update(uint32_t dt_ms, float espacio_m_now) {
  if(g_state == TI_IDLE || g_state == TI_DONE) return;

  float delta_m = espacio_m_now - g_last_espacio_m;
  if(delta_m < 0) delta_m = 0;
  g_last_espacio_m = espacio_m_now;

  switch(g_state) {
    case TI_ARMED:
      return;

    case TI_RUN: {
      bool pass_done = false;

      // 1) correr hasta terminar la pasada
      if(g_cfg.mode == TI_PASS_BY_TIME) {

        // Guardar previo para calcular "consumido real"
        uint32_t prev_ms = g_pass_remain_ms;

        if(g_pass_remain_ms > dt_ms) g_pass_remain_ms -= dt_ms;
        else g_pass_remain_ms = 0;

        uint32_t used_ms = prev_ms - g_pass_remain_ms;   // consumido real
        g_total_work_ms += used_ms;                      // ✅ solo una vez
        g_total_dist_m  += delta_m;                      // distancia real durante RUN

        overlay_set_time_remain(g_pass_remain_ms);

        if(g_pass_remain_ms == 0) pass_done = true;

      } else {
        // En distancia: el tiempo total es el tiempo real de trabajo en RUN
        g_total_work_ms += dt_ms;

        float used_m = delta_m;
        if(used_m > g_pass_remain_m) used_m = g_pass_remain_m;

        g_pass_remain_m -= delta_m;
        if(g_pass_remain_m <= 0.0f) {
          g_pass_remain_m = 0.0f;
          pass_done = true;
        }

        g_total_dist_m += used_m;                        // ✅ solo una vez (clamp)
        overlay_set_dist_remain(g_pass_remain_m);
      }

      if(!pass_done) return;

      // 2) terminó la pasada -> descontar pasadas
      if(g_pass_left > 0) g_pass_left--;
      overlay_update_cfg_strings();

      // 3) si todavía quedan pasadas en ESTA serie -> micropausa (o directo a la próxima)
      if(g_pass_left > 0) {
        if(need_rest_pass()) enter_rest_pass();
        else enter_run();
        return;
      }

      // 4) terminó la serie actual (ya no quedan pasadas)
      if(need_rest_series()) {
        g_series_remaining--;
        g_pass_left = g_cfg.num_passes;
        overlay_update_cfg_strings();

        if(g_cfg.rest_series_ms > 0) enter_rest_series();
        else enter_run();
        return;
      }

      // 5) no hay más series -> FIN
      enter_done();
      return;
    }

    
    case TI_REST_PASS: {
      if(g_rest_remain_ms > dt_ms) g_rest_remain_ms -= dt_ms;
      else g_rest_remain_ms = 0;

      // Actualiza el cartel "PASADA EN PAUSA" + el mm:ss que ve el usuario
      overlay_set_pausa(true, false, "PASADA EN PAUSA", g_rest_remain_ms);

      if(g_rest_remain_ms == 0) {
        enter_run();
      }
      return;
    }

    case TI_REST_SERIES: {
      if(g_rest_remain_ms > dt_ms) g_rest_remain_ms -= dt_ms;
      else g_rest_remain_ms = 0;

      // Actualiza el cartel "SERIE EN PAUSA" + el mm:ss que ve el usuario
      overlay_set_pausa(true, true, "SERIE EN PAUSA", g_rest_remain_ms);

      if(g_rest_remain_ms == 0) {
        enter_run();
      }

    default:
      return;
    }  
  }
}

void ti_apply_overlay_to_ui(ui_state_t *out) {
  if(!out) return;

  if(g_state != TI_IDLE) {
    // contadores/config visibles
    strlcpy(out->intervalSeriesStr,  g_ov_seriesStr,   sizeof(out->intervalSeriesStr));
    strlcpy(out->intervalPasadasStr, g_ov_passStr,     sizeof(out->intervalPasadasStr));
    strlcpy(out->intervalSetpointStr,g_ov_setpointStr, sizeof(out->intervalSetpointStr));
  } else {
    out->intervalSeriesStr[0] = 0;
    out->intervalPasadasStr[0] = 0;
    out->intervalSetpointStr[0] = 0;
  }

  // setpoint visible solo antes de empezar
  out->intervalShowSetpoint = (g_state == TI_ARMED);

  // pausa
  out->pausaVisible = g_ov_pausaVisible;
  if(g_ov_pausaVisible) {
    out->pausaIsSerie = g_ov_pausaIsSerie;
    strlcpy(out->pausaTitle, g_ov_pausaTitle, sizeof(out->pausaTitle));
    strlcpy(out->pausaStr,   g_ov_pausaStr,   sizeof(out->pausaStr));
  }

  // override de tiempo (solo modo TIEMPO durante RUN)
  if(g_ov_timeOverride) {
    strlcpy(out->trainTimeStr, g_ov_timeStr, sizeof(out->trainTimeStr));
  }

  // override de distancia (solo modo DISTANCIA durante RUN)
  if(g_ov_distOverride) {
    strlcpy(out->distStr, g_ov_distStr, sizeof(out->distStr));
  }  

  // ✅ En ARMED (antes de empezar), mostrarmos el setpoint en el mismo label del tiempo
  if(g_state == TI_ARMED) {
    if(g_cfg.mode == TI_PASS_BY_TIME) {
      strlcpy(out->trainTimeStr, g_ov_setpointStr, sizeof(out->trainTimeStr));
    } else {
      strlcpy(out->trainTimeStr, "00:00:00", sizeof(out->trainTimeStr));
      strlcpy(out->distStr, g_ov_setpointStr, sizeof(out->distStr));
    }
  }
  out->intervalIsDistance = (g_cfg.mode == TI_PASS_BY_TIME) ? false : true;
  out->forceBarsVisible = (g_workout_frozen || (g_state == TI_DONE));
}

void ti_reset_stats(void)
{
  // descongelar
  g_workout_frozen = false;
  memset(&g_workout_final, 0, sizeof(g_workout_final));

  // reset de acumuladores internos (son static acá)
  g_total_work_ms = 0;
  g_total_dist_m  = 0.0f;

  // si usás kcal live global:
  g_kcal_total_live = 0.0f;

  // volver a idle / parar entrenamiento
  ti_stop();
}

