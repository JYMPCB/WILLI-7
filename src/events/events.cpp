#include <Arduino.h>
#include <lvgl.h>
#include "events.h"

#include "../ui/ui.h"
#include "../training/training_interval.h"
#include "../app/app_globals.h"

#include <time.h>

#ifndef beep
#define beep 19
#endif

static inline void ui_req(uint32_t f);

void service_aceptar(lv_event_t * e)
{
  (void)e;
  ui_req(UI_REQ_SERVICE_ACCEPT);
}

void service_postergar(lv_event_t * e)
{
  (void)e;
  ui_req(UI_REQ_SERVICE_POSTPONE);
}

// Helper: pedir refresh de UI (LVGL se toca en ui_refresh)
static inline void ui_req(uint32_t f) { g_ui_req_flags |= f; }

// ----------------------------------------------------------
// ------------------------ WIFI ----------------------------
// ----------------------------------------------------------

// Botón único WiFi (no toca LVGL acá: ui_refresh lee dropdown/pass)
void wifi_btn_toggle(lv_event_t * e)
{
  if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  tone(beep, 1000, 50);
  tone(beep, 1300, 50);

  ui_req(UI_REQ_WIFI_TOGGLE);
}

// Mantengo estas por compatibilidad si SquareLine las llama,
// pero NO tocan LVGL: solo marcan acciones.
void conect_wifi(lv_event_t * e)
{
  (void)e;
  ui_req(UI_REQ_WIFI_TOGGLE);  // mismo comportamiento: toggle desde UI refresh
}

void disconect_wifi(lv_event_t * e)
{
  (void)e;
  // Forzamos desconexión sin leer UI
  snprintf(cfg_info, sizeof(cfg_info), "Desconectando...");
  cfg_do_disconnect = true;
}

void config_loaded(lv_event_t * e)
{
  (void)e;

  tone(beep, 1000, 150);

  habConfig   = true;
  cfg_entered = true;

  // Pedir scan (ui_refresh lo dispara)
  ui_req(UI_REQ_WIFI_SCAN);
}

void change_to_home(lv_event_t * e)
{
  (void)e;
  tone(beep, 900, 150);
  habConfig = false;
}

void chart_loaded(lv_event_t * e)
{
  (void)e;
  habChart = 1;
}

// ----------------------------------------------------------
// ------------------ ENTRADA A MODOS ------------------------
// ----------------------------------------------------------

// INGRESA A CONFIGURAR INTERVALADO
void btnProgramaOn(lv_event_t * e)
{
  (void)e;
  tone(beep, 1500, 200);
  tone(beep, 1200, 100);

  g_train_mode = TRAIN_INTERVAL;

  // venimos a configurar: apagá cualquier estado previo
  ti_stop();

  // opcional: ocultar cartel pausa (UI state protegido)
  xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
  g_ui.pausaVisible = false;
  xSemaphoreGive(g_ui_mutex);

  // Forzar que la pantalla PROGRAMADO quede coherente
  ui_req(UI_REQ_PROG_MODE_REFRESH | UI_REQ_PROG_REFRESH);
}

void btnLibreOn(lv_event_t * e)
{
  (void)e;
  tone(beep, 1200, 200);
  tone(beep, 1500, 100);

  g_train_mode = TRAIN_FREE;

  // apagar intervalado por las dudas
  ti_stop();

  // reset sesión
  espacio = 0.0f;
  tiempoHour = tiempoMin = tiempoSec = 0;

  g_speed_reset_req = true;
}

// ----------------------------------------------------------
// ------------------ BOTONES MODO PASADA --------------------
// ----------------------------------------------------------

void btnTimeModeClicked(lv_event_t * e)
{
  (void)e;

  g_interval_is_dist = false; // por tiempo
  ui_req(UI_REQ_PROG_MODE_REFRESH | UI_REQ_PROG_REFRESH);
}

void btnDistanceModeClicked(lv_event_t * e)
{
  (void)e;

  g_interval_is_dist = true;  // por distancia
  ui_req(UI_REQ_PROG_MODE_REFRESH | UI_REQ_PROG_REFRESH);
}

// ----------------------------------------------------------
// ---------------------- SETPOINTS --------------------------
// ----------------------------------------------------------

void btnSetPointSeriesPlus(lv_event_t * e)
{
  (void)e;
  if(setPointSeries < SET_POINT_SERIES_MAX) setPointSeries++;
  else setPointSeries = 0;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointSeriesMinus(lv_event_t * e)
{
  (void)e;
  if(setPointSeries > 0) setPointSeries--;
  else setPointSeries = SET_POINT_SERIES_MAX;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointPasadasPlus(lv_event_t * e)
{
  (void)e;
  if(setPointPasadas < SET_POINT_PASADAS_MAX) setPointPasadas++;
  else setPointPasadas = 1;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointPasadasMinus(lv_event_t * e)
{
  (void)e;
  if(setPointPasadas > 1) setPointPasadas--;
  else setPointPasadas = SET_POINT_PASADAS_MAX;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointMacroPause(lv_event_t * e)
{
  (void)e;
  if(setPointMacroPausaMin < SET_POINT_MACROPAUSE_MAX) setPointMacroPausaMin++;
  else setPointMacroPausaMin = 1;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointMicroPauseMin(lv_event_t * e)
{
  (void)e;
  if(setPointMicroPausaMin < SET_POINT_MICROPAUSE_MAX) setPointMicroPausaMin++;
  else setPointMicroPausaMin = 0;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointMicroPauseSeg(lv_event_t * e)
{
  (void)e;

  uint8_t s = setPointMicroPausaSeg;
  if(s <= 55) s += 5;

  if(s >= 60 && setPointMicroPausaMin == 0) s = 5;
  else if(s >= 60 && setPointMicroPausaMin > 0) s = 0;

  setPointMicroPausaSeg = s;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointDistancePasada(lv_event_t * e)
{
  (void)e;

  uint16_t d = setPointDistancePasada;
  if(d < SET_POINT_DISTANCE_PASADA_MAX) d = (uint16_t)(d + 100);
  else d = 100;

  setPointDistancePasada = d;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointTimePasadaMin(lv_event_t * e)
{
  (void)e;
  if(setPointTimePasadaMin < SET_POINT_TIME_PASADA_MAX) setPointTimePasadaMin++;
  else setPointTimePasadaMin = 0;

  ui_req(UI_REQ_PROG_REFRESH);
}

void btnSetPointTimePasadaSeg(lv_event_t * e)
{
  (void)e;

  uint8_t s = setPointTimePasadaSeg;
  if(s <= 55) s += 5;

  if(s >= 60 && setPointTimePasadaMin == 0) s = 5;
  else if(s >= 60 && setPointTimePasadaMin > 0) s = 0;

  setPointTimePasadaSeg = s;

  ui_req(UI_REQ_PROG_REFRESH);
}

// ----------------------------------------------------------
// ---------------------- ACEPTAR OK -------------------------
// ----------------------------------------------------------

void btnProgramOk(lv_event_t * e)
{
  (void)e;

  tone(beep, 1500, 100);
  tone(beep, 1500, 100);

  g_train_mode = TRAIN_INTERVAL;

  // NO leer LVGL acá, usar flag lógico
  bool swDist = g_interval_is_dist;

  ti_cfg_t cfg = {0};

  cfg.num_series = setPointSeries;
  cfg.num_passes = setPointPasadas;

  uint32_t macroPauseMin = setPointMacroPausaMin;
  uint32_t microPauseMin = setPointMicroPausaMin;
  uint32_t microPauseSec = setPointMicroPausaSeg;

  cfg.rest_series_ms = macroPauseMin * 60UL * 1000UL;
  cfg.rest_pass_ms   = (microPauseMin * 60UL + microPauseSec) * 1000UL;

  if(swDist) {
    cfg.mode        = TI_PASS_BY_DIST;
    cfg.pass_dist_m = setPointDistancePasada; // METROS
    cfg.pass_time_ms = 0;
  } else {
    cfg.mode = TI_PASS_BY_TIME;
    uint32_t passMin = setPointTimePasadaMin;
    uint32_t passSec = setPointTimePasadaSeg;
    cfg.pass_time_ms = (passMin * 60UL + passSec) * 1000UL;
    cfg.pass_dist_m  = 0;
  }

  // guardar última config y dejar armado
  ti_apply_cfg(&cfg);
  ti_arm();  // espera inicio inteligente

  // reset sesión
  espacio = 0.0f;
  tiempoHour = tiempoMin = tiempoSec = 0;

  g_speed_reset_req = true;
}

void reinit_ent(lv_event_t * e)
{
  (void)e;

  g_reset_req = true;

  if(g_mathTaskHandle)  xTaskNotifyGive(g_mathTaskHandle);
  if(g_speedTaskHandle) xTaskNotifyGive(g_speedTaskHandle);

  ti_reset_stats();

  tone(beep, 1500, 200);
  tone(beep, 1200, 100);
}
