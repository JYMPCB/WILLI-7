#include <Arduino.h>
#include <lvgl.h>
#include "ui_refresh.h"
#include "ui_state.h"
#include "../training/training_interval.h"
#include "../app/app_globals.h"
#include "../ui/ui.h"
#include "../rgb/rgb_gpio.h"
#include <time.h>

static bool first_refresh = true;

// Flag RAM para no abrir el popup 20 veces en el refresh
static bool s_service_evaluated_this_boot = false;  // evaluar vencimiento (1 vez)
static bool s_service_popup_attempted_this_boot = false; // mostrar popup (1 vez)

static void service_popup_show() {
  // Mostrar panel (popup)
  lv_obj_clear_flag(ui_pnlServiceOdo, LV_OBJ_FLAG_HIDDEN);
}

static void service_popup_hide() {
  lv_obj_add_flag(ui_pnlServiceOdo, LV_OBJ_FLAG_HIDDEN);
}

// helper: hora válida si > 2023 aprox
static bool net_time_valid()
{
  time_t now = time(nullptr);
  return (now > 1700000000);
}

static void service_popup_update_text()
{
  char buf[128];

  if(net_time_valid()) {
    time_t now = time(nullptr);
    g_service.formatPopupText(buf, sizeof(buf), now);  // <- función real del ServiceMgr
  } else {
    strlcpy(buf, "Mantenimiento requerido.\n(Sin hora valida)", sizeof(buf));
  }

  lv_label_set_text(ui_lblOdoMaquina2, buf);
}

//---------ARO RGB-------------
extern lv_obj_t * ui_rgbRing;

static inline void ring_apply_color(uint8_t r, uint8_t g, uint8_t b){
  if(!ui_rgbRing) return;

  static uint32_t last_rgb = 0xFFFFFFFF;
  uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  if(rgb == last_rgb) return;
  last_rgb = rgb;

  lv_color_t col = lv_color_make(r,g,b);
  lv_obj_set_style_bg_img_recolor(ui_rgbRing, col, LV_PART_MAIN);
  lv_obj_set_style_bg_img_recolor_opa(ui_rgbRing, LV_OPA_COVER, LV_PART_MAIN);
}

static void ui_net_time_update(ui_state_t &s)
{
  if(!wifi_ok) {
    s.showNetTime = false;
    return;
  }

  if(!net_time_valid()) {
    s.showNetTime = false;
    return;
  }

  s.showNetTime = true;

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  strftime(s.timeStr, sizeof(s.timeStr), "%H:%M", &t);
  strftime(s.dateStr, sizeof(s.dateStr), "%d/%m/%Y", &t);
}

// --------- iconos wifi ----------
static void wifi_icons_apply(bool ok)
{
  lv_color_t c = ok ? lv_color_hex(0xFFBF00) : lv_color_hex(0x525552);

  if(ui_imgWifi)  lv_obj_set_style_img_recolor(ui_imgWifi,  c, LV_PART_MAIN | LV_STATE_DEFAULT);
  if(ui_imgWifi2) lv_obj_set_style_img_recolor(ui_imgWifi2, c, LV_PART_MAIN | LV_STATE_DEFAULT);
}

//helper para boton conectar wifi en pantalla configuracion
static void wifi_btn_apply(bool ok)
{
  if(!ui_btnWifi) return;

  if(ok) lv_obj_add_state(ui_btnWifi, LV_STATE_CHECKED);
  else   lv_obj_clear_state(ui_btnWifi, LV_STATE_CHECKED);
}

// ---------------- PROGRAMADO helpers ----------------
static void programado_apply_mode(bool dist)
{
  if(!ui_btnDistanceMode || !ui_btnTimeMode) return;

  if(dist){
    lv_obj_add_state(ui_btnDistanceMode, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_btnTimeMode, LV_STATE_CHECKED);

    if(ui_ContainerTiempoPasadaMin) lv_obj_add_flag(ui_ContainerTiempoPasadaMin, LV_OBJ_FLAG_HIDDEN);
    if(ui_ContainerTiempoPasadaSeg) lv_obj_add_flag(ui_ContainerTiempoPasadaSeg, LV_OBJ_FLAG_HIDDEN);
    if(ui_ContainerDistancePasada)  lv_obj_clear_flag(ui_ContainerDistancePasada, LV_OBJ_FLAG_HIDDEN);

    if(ui_lblDistanceModeOk) lv_obj_clear_flag(ui_lblDistanceModeOk, LV_OBJ_FLAG_HIDDEN);
    if(ui_lblTimeModeOk)     lv_obj_add_flag(ui_lblTimeModeOk, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_state(ui_btnTimeMode, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_btnDistanceMode, LV_STATE_CHECKED);

    if(ui_ContainerTiempoPasadaMin) lv_obj_clear_flag(ui_ContainerTiempoPasadaMin, LV_OBJ_FLAG_HIDDEN);
    if(ui_ContainerTiempoPasadaSeg) lv_obj_clear_flag(ui_ContainerTiempoPasadaSeg, LV_OBJ_FLAG_HIDDEN);
    if(ui_ContainerDistancePasada)  lv_obj_add_flag(ui_ContainerDistancePasada, LV_OBJ_FLAG_HIDDEN);

    if(ui_lblTimeModeOk)     lv_obj_clear_flag(ui_lblTimeModeOk, LV_OBJ_FLAG_HIDDEN);
    if(ui_lblDistanceModeOk) lv_obj_add_flag(ui_lblDistanceModeOk, LV_OBJ_FLAG_HIDDEN);
  }
}

static void programado_refresh_labels()
{
  if(ui_lblSetPointNumSeries)      lv_label_set_text_fmt(ui_lblSetPointNumSeries, "%u", (unsigned)setPointSeries);
  if(ui_lblSetPointNumPasadas)     lv_label_set_text_fmt(ui_lblSetPointNumPasadas, "%u", (unsigned)setPointPasadas);

  if(ui_lblSetPointMacroPause)     lv_label_set_text_fmt(ui_lblSetPointMacroPause, "%u min", (unsigned)setPointMacroPausaMin);
  if(ui_lblSetPointMicropausaMin)  lv_label_set_text_fmt(ui_lblSetPointMicropausaMin, "%u min", (unsigned)setPointMicroPausaMin);
  if(ui_lblSetPointMicropausaSeg)  lv_label_set_text_fmt(ui_lblSetPointMicropausaSeg, "%u s",   (unsigned)setPointMicroPausaSeg);

  if(ui_lblSetPointDistancePasada) lv_label_set_text_fmt(ui_lblSetPointDistancePasada, "%u mt", (unsigned)setPointDistancePasada);
  if(ui_lblSetPointTimePasadaMin)  lv_label_set_text_fmt(ui_lblSetPointTimePasadaMin, "%u min", (unsigned)setPointTimePasadaMin);
  if(ui_lblSetPointTimePasadaSeg)  lv_label_set_text_fmt(ui_lblSetPointTimePasadaSeg, "%u s",   (unsigned)setPointTimePasadaSeg);

  if(ui_lblInfoTraining){
    if(g_interval_is_dist){
      lv_label_set_text_fmt(ui_lblInfoTraining,
        "Series: %u (pausa %u min) | Pasadas: %u (pausa %u:%02u) | Modo: Distancia %u m",
        (unsigned)setPointSeries,
        (unsigned)setPointMacroPausaMin,
        (unsigned)setPointPasadas,
        (unsigned)setPointMicroPausaMin,
        (unsigned)setPointMicroPausaSeg,
        (unsigned)setPointDistancePasada
      );
    } else {
      lv_label_set_text_fmt(ui_lblInfoTraining,
        "Series: %u (pausa %u min) | Pasadas: %u (pausa %u:%02u) | Modo: Tiempo %u:%02u",
        (unsigned)setPointSeries,
        (unsigned)setPointMacroPausaMin,
        (unsigned)setPointPasadas,
        (unsigned)setPointMicroPausaMin,
        (unsigned)setPointMicroPausaSeg,
        (unsigned)setPointTimePasadaMin,
        (unsigned)setPointTimePasadaSeg
      );
    }
  }
}

// TIMER LVGL: acá van TODAS las sentencias LVGL
void ui_refresh_cb(lv_timer_t *t)
{
  (void)t;

  ui_state_t s;

  xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
  s = g_ui;
  xSemaphoreGive(g_ui_mutex);

  // --------- CACHE ----------
  static bool last_headerHidden   = false;
  static bool last_settingsHidden = false;

  static bool last_showNetTime = false;
  static char last_timeStr[8]  = "";
  static char last_dateStr[11] = "";

  static char last_speedStr[16] = "";
  static int  last_cursorAngle  = -1;

  static char last_ritmoStr[16] = "";

  static char last_distStr[16] = "";
  static char last_distUnit[4]  = "";

  static char last_calStr[16]   = "";
  static char last_trainTimeStr[16] = "";

  static bool last_pausaVisible = false;
  static bool last_pausaIsSerie = false;
  static char last_pausaTitle[24] = "";
  static char last_pausaStr[8]  = "";

  static char last_intervalSeriesStr[8]  = "";
  static char last_intervalPasadasStr[8] = "";

  static bool last_habConfig = false;
  static char last_cfg_info[64] = "";
  static char last_cfg_ip[24]   = "";
  static char last_cfg_ssid[33] = "";

  static bool last_wifi_ok = false;

  static char last_odoStr[32] = "";

  // Copias locales
  char info_l[64], ip_l[24], ssid_l[33];
  strlcpy(info_l, cfg_info, sizeof(info_l));
  strlcpy(ip_l,   cfg_ip,   sizeof(ip_l));
  strlcpy(ssid_l, cfg_ssid, sizeof(ssid_l));

  /* --- OTA --- */

  // 1) Overlay visible solo durante OTA
  if (ui_otaOverlay) {
    if (g_ota_active) lv_obj_clear_flag(ui_otaOverlay, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(ui_otaOverlay, LV_OBJ_FLAG_HIDDEN);
  }

  // 2) Progreso
  if (ui_barOta) lv_bar_set_value(ui_barOta, g_ota_progress, LV_ANIM_OFF);

  // 3) Status
  if (ui_lblOtaStatus) lv_label_set_text(ui_lblOtaStatus, g_ota_status);
  // Durante OTA, NO refrescar nada más (evita glitches/carga)
  if (g_ota_active) return;
  
  if(first_refresh) {
    first_refresh = false;

    last_cfg_ssid[0] = '\0';
    last_timeStr[0]  = '\0';
    last_dateStr[0]  = '\0';
    last_wifi_ok     = !wifi_ok;

    lv_label_set_text(ui_lblNetName, "");
    lv_label_set_text(ui_lblTime, "");
    lv_label_set_text(ui_lblDate, "");

    // Si entrás directo a PROGRAMADO, dejalo coherente
    programado_apply_mode(g_interval_is_dist);
    programado_refresh_labels();

    lv_obj_add_event_cb(ui_btnServiceAceptar, service_aceptar, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnServicePostergar, service_postergar, LV_EVENT_CLICKED, NULL);
    service_popup_hide();
  }
  

  // HOME: indicador de update
  if (ui_dotOta) {
      if (g_ota_available)
          lv_obj_clear_flag(ui_dotOta, LV_OBJ_FLAG_HIDDEN);
      else
          lv_obj_add_flag(ui_dotOta, LV_OBJ_FLAG_HIDDEN);
  }

  // CONFIG: estado textual
  if (ui_lblOtaStatus) {
      lv_label_set_text(ui_lblOtaStatus, g_ota_status);
  }

  // CONFIG: notas
  if (ui_lblOtaNotes) {
      if (g_ota_available)
          lv_label_set_text(ui_lblOtaNotes, g_ota_notes);
      else
          lv_label_set_text(ui_lblOtaNotes, "");
  }

  // CONFIG: progreso
  if (ui_barOta) {
      if (g_ota_active) {
          lv_obj_clear_flag(ui_barOta, LV_OBJ_FLAG_HIDDEN);
          lv_bar_set_value(ui_barOta, g_ota_progress, LV_ANIM_OFF);
      } else {
          lv_bar_set_value(ui_barOta, 0, LV_ANIM_OFF);
      }
  }

  // CONFIG: botón actualizar
  if (ui_btnOtaUpdate) {
      if (g_ota_available && !g_ota_active)
          lv_obj_clear_state(ui_btnOtaUpdate, LV_STATE_DISABLED);
      else
          lv_obj_add_state(ui_btnOtaUpdate, LV_STATE_DISABLED);
  }

  // ============================================================
  // ------------- CONSUMIR REQUEST FLAGS (UI marks) -------------
  // ============================================================
  uint32_t req = g_ui_req_flags;
  if(req) g_ui_req_flags = 0;

   // ============================================================
  // ---------------- SERVICE BUTTONS (UI side) ------------------
  // ============================================================
  if(req & UI_REQ_SERVICE_ACCEPT) {
    time_t now = time(nullptr);

    if(net_time_valid()) {
      g_service.onAccept(now);   // resetea a 45 días
    } else {
      // sin hora válida: no se puede reprogramar, queda postergado
      g_service.onPostpone();
    }

    service_popup_hide(); // LVGL acá OK
  }

  if(req & UI_REQ_SERVICE_POSTPONE) {
    g_service.onPostpone();
    service_popup_hide(); // LVGL acá OK
  }

    // HOME: puntito si hay update
  if (ui_dotOta) {
    lv_obj_add_flag(ui_dotOta, g_ota_available ? LV_OBJ_FLAG_HIDDEN : 0); // si usás flags al revés ajustalo
    if (g_ota_available) lv_obj_clear_flag(ui_dotOta, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_dotOta, LV_OBJ_FLAG_HIDDEN);
  }

  // Config: status + notas + progreso
  if (ui_lblOtaStatus) lv_label_set_text(ui_lblOtaStatus, g_ota_status);

  if (ui_lblOtaNotes) {
    if (g_ota_available) lv_label_set_text(ui_lblOtaNotes, g_ota_notes);
    else lv_label_set_text(ui_lblOtaNotes, "");
  }

  if (ui_barOta) {
    if (g_ota_active) {
      lv_obj_clear_flag(ui_barOta, LV_OBJ_FLAG_HIDDEN);
      lv_bar_set_value(ui_barOta, g_ota_progress, LV_ANIM_OFF);
    } else {
      // opcional: ocultar o dejar en 0
      // lv_obj_add_flag(ui_barOta, LV_OBJ_FLAG_HIDDEN);
      lv_bar_set_value(ui_barOta, 0, LV_ANIM_OFF);
    }
  }

  // Botón update habilitado solo si hay update y no está en progreso
  if (ui_btnOtaUpdate) {
    if (g_ota_available && !g_ota_active) lv_obj_clear_state(ui_btnOtaUpdate, LV_STATE_DISABLED);
    else lv_obj_add_state(ui_btnOtaUpdate, LV_STATE_DISABLED);
  }


  // --- WIFI: scan al entrar ---
  if(req & UI_REQ_WIFI_SCAN) {
    cfg_need_scan = true;
    snprintf(cfg_info, sizeof(cfg_info), "Buscando redes...");
  }

  // --- WIFI: toggle (leer LVGL acá, no en events) ---
  if(req & UI_REQ_WIFI_TOGGLE) {
    if(wifi_ok) {
      snprintf(cfg_info, sizeof(cfg_info), "Desconectando...");
      cfg_do_disconnect = true;
    } else {
      const char *pass = (ui_passArea) ? lv_textarea_get_text(ui_passArea) : "";

      char ssid[33] = {0};
      if(ui_Dropdown1) lv_dropdown_get_selected_str(ui_Dropdown1, ssid, sizeof(ssid));

      if(strcmp(ssid, "SIN REDES") == 0 || strlen(ssid) == 0) {
        snprintf(cfg_info, sizeof(cfg_info), "Selecciona una red");
      } else {
        strlcpy(pending_ssid, ssid, sizeof(pending_ssid));
        strlcpy(pending_pass, pass, sizeof(pending_pass));
        snprintf(cfg_info, sizeof(cfg_info), "Conectando a %s...", pending_ssid);
        cfg_do_connect = true;
      }
    }
  }

  // --- PROGRAMADO ---
  if(req & UI_REQ_PROG_MODE_REFRESH) {
    programado_apply_mode(g_interval_is_dist);
  }
  if(req & UI_REQ_PROG_REFRESH) {
    programado_refresh_labels();
  }

  // --------- visibilidad ----------
  if(s.forceBarsVisible) {
    lv_obj_clear_flag(ui_Group_Settings_Down, LV_OBJ_FLAG_HIDDEN);
  }
  else {
    if(s.headerHidden != last_headerHidden) {
      last_headerHidden = s.headerHidden;
    }

    if(s.settingsHidden != last_settingsHidden) {
      last_settingsHidden = s.settingsHidden;
      if(s.settingsHidden) lv_obj_add_flag(ui_Group_Settings_Down, LV_OBJ_FLAG_HIDDEN);
      else                 lv_obj_clear_flag(ui_Group_Settings_Down, LV_OBJ_FLAG_HIDDEN);
    }

    if(s.showReinitBtn) lv_obj_clear_flag(ui_groupReinitbtn, LV_OBJ_FLAG_HIDDEN);
    else                lv_obj_add_flag(ui_groupReinitbtn, LV_OBJ_FLAG_HIDDEN);
  }

  // --------- hora/fecha y SSID WIFI----------
  if(wifi_ok) {
    if(strcmp(ssid_l, last_cfg_ssid) != 0) {
      strlcpy(last_cfg_ssid, ssid_l, sizeof(last_cfg_ssid));
      lv_label_set_text(ui_lblNetName, ssid_l);
    }
  } else {
    if(last_cfg_ssid[0] != '\0') {
      last_cfg_ssid[0] = '\0';
      lv_label_set_text(ui_lblNetName, "");
    }
  }

  ui_net_time_update(s);

  // ============================================================
  // --------- SERVICE 45 DIAS (solo boot + solo HOME) ----------
  // ============================================================
  const bool is_home = (habConfig == 0);

  // 1) Evaluar vencimiento SOLO 1 vez por boot, SOLO si:
  //    - estamos en HOME
  //    - hay hora válida (NTP)
  if(!s_service_evaluated_this_boot && is_home && net_time_valid()) {
    time_t now = time(nullptr);
    g_service.evaluate_on_boot(now);   // si venció -> deja pending=true
    s_service_evaluated_this_boot = true;
  }

  // 2) Mostrar popup SOLO 1 vez por boot y SOLO en HOME
  if(!s_service_popup_attempted_this_boot && is_home) {
    if(g_service.pending()) {
      service_popup_update_text();
      service_popup_show();
    } else {
      service_popup_hide();
    }
    s_service_popup_attempted_this_boot = true;
  }


  if(s.showNetTime) {
    if(strcmp(s.timeStr, last_timeStr) != 0) {
      strlcpy(last_timeStr, s.timeStr, sizeof(last_timeStr));
      lv_label_set_text(ui_lblTime, s.timeStr);
    }
    if(strcmp(s.dateStr, last_dateStr) != 0) {
      strlcpy(last_dateStr, s.dateStr, sizeof(last_dateStr));
      lv_label_set_text(ui_lblDate, s.dateStr);
    }
  } else {
    if(last_timeStr[0] != '\0') {
      last_timeStr[0] = '\0';
      lv_label_set_text(ui_lblTime, "");
    }
    if(last_dateStr[0] != '\0') {
      last_dateStr[0] = '\0';
      lv_label_set_text(ui_lblDate, "");
    }
  }

  // --------- velocidad + cursor ----------
  char speed_to_show[16];

  if(g_workout_frozen) {
    snprintf(speed_to_show, sizeof(speed_to_show), "%.1f", (double)g_workout_final.avg_speed_kmh);
  } else {
    strlcpy(speed_to_show, s.speedStr, sizeof(speed_to_show));
  }

  if(speed_to_show[0] != '\0' && strcmp(speed_to_show, last_speedStr) != 0) {
    strlcpy(last_speedStr, speed_to_show, sizeof(last_speedStr));
    lv_label_set_text(ui_lblSpeedNumber, speed_to_show);

    if(s.cursorAngle != last_cursorAngle) {
      last_cursorAngle = s.cursorAngle;
      lv_img_set_angle(ui_imgCursorSpeed, s.cursorAngle);
    }
  }

  // --------- ritmo ----------
  char ritmo_to_show[16];

  if(g_workout_frozen) {
    float pace = g_workout_final.avg_pace_min_km;
    if(pace < 0) pace = 0;

    int mm = (int)pace;
    int ss = (int)((pace - mm) * 60.0f + 0.5f);
    if(ss >= 60) { ss -= 60; mm++; }

    snprintf(ritmo_to_show, sizeof(ritmo_to_show), "%02d:%02d", mm, ss);
  } else {
    strlcpy(ritmo_to_show, s.ritmoStr, sizeof(ritmo_to_show));
  }

  if(ritmo_to_show[0] != '\0' && strcmp(ritmo_to_show, last_ritmoStr) != 0) {
    strlcpy(last_ritmoStr, ritmo_to_show, sizeof(last_ritmoStr));
    lv_label_set_text(ui_LabelVarRitm, ritmo_to_show);
  }

  //---------ARO RGB-------------
  ring_apply_color(g_rgb_target_r, g_rgb_target_g, g_rgb_target_b);

  // --------- distancia ----------
  char dist_to_show[16];

  if(g_workout_frozen) {
    snprintf(dist_to_show, sizeof(dist_to_show), "%.0f", (double)g_workout_final.total_dist_m);
  } else {
    strlcpy(dist_to_show, s.distStr, sizeof(dist_to_show));
  }

  if(strcmp(dist_to_show, last_distStr) != 0) {
    strlcpy(last_distStr, dist_to_show, sizeof(last_distStr));
    lv_label_set_text(ui_lblDistanceNumber, dist_to_show);
  }

  // --------- unidad ----------
  if(strcmp(s.distUnit, last_distUnit) != 0) {
    strlcpy(last_distUnit, s.distUnit, sizeof(last_distUnit));
    lv_label_set_text(ui_lblDistanceUnit, s.distUnit);
  }

  // --------- calorías ----------
  char cal_to_show[16];

  if(g_workout_frozen) {
    snprintf(cal_to_show, sizeof(cal_to_show), "%.0f", (double)g_workout_final.total_kcal);
  } else {
    strlcpy(cal_to_show, s.calStr, sizeof(cal_to_show));
  }

  if(strcmp(cal_to_show, last_calStr) != 0) {
    strlcpy(last_calStr, cal_to_show, sizeof(last_calStr));
    lv_label_set_text(ui_lblCaloriasNumber, cal_to_show);
  }

  // --------- tiempo / setpoint (ui_lblTimeNumber) ----------
  char time_to_show[16];

  if(g_workout_frozen) {
    uint32_t total_s = g_workout_final.total_time_ms / 1000;

    uint32_t hh = total_s / 3600;
    uint32_t mm = (total_s % 3600) / 60;
    uint32_t ss = total_s % 60;

    snprintf(time_to_show, sizeof(time_to_show),
            "%02lu:%02lu:%02lu",
            (unsigned long)hh,
            (unsigned long)mm,
            (unsigned long)ss);
  }
  else {
    const char *src = s.trainTimeStr;

    if(s.intervalShowSetpoint && !s.intervalIsDistance && s.intervalSetpointStr[0] != 0) {
      src = s.intervalSetpointStr;
    }

    strlcpy(time_to_show, src, sizeof(time_to_show));
  }

  if(strcmp(time_to_show, last_trainTimeStr) != 0) {
    strlcpy(last_trainTimeStr, time_to_show, sizeof(last_trainTimeStr));
    lv_label_set_text(ui_lblTimeNumber, time_to_show);
  }

  // --------- pausa (cartel único) ----------
  if(s.pausaVisible != last_pausaVisible) {
    last_pausaVisible = s.pausaVisible;
    if(s.pausaVisible) lv_obj_clear_flag(ui_groupPausa, LV_OBJ_FLAG_HIDDEN);
    else               lv_obj_add_flag(ui_groupPausa, LV_OBJ_FLAG_HIDDEN);
  }

  if(s.pausaVisible) {

    if(s.pausaIsSerie != last_pausaIsSerie) {
      last_pausaIsSerie = s.pausaIsSerie;
      lv_color_t c = s.pausaIsSerie ? lv_color_hex(0xCC2E2E) : lv_color_hex(0xE0C100);
      lv_obj_set_style_bg_color(ui_groupPausa, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(strcmp(s.pausaTitle, last_pausaTitle) != 0) {
      strlcpy(last_pausaTitle, s.pausaTitle, sizeof(last_pausaTitle));
      lv_label_set_text(ui_lblTittlePausa, s.pausaTitle);
    }

    if(strcmp(s.pausaStr, last_pausaStr) != 0) {
      strlcpy(last_pausaStr, s.pausaStr, sizeof(last_pausaStr));
      lv_label_set_text(ui_lblPausa, s.pausaStr);
    }
  }

  // --------- pantalla config ----------
  if((habConfig == 1) != last_habConfig) {
    last_habConfig = (habConfig == 1);

    last_cfg_info[0] = '\0';
    last_cfg_ip[0]   = '\0';
    last_cfg_ssid[0] = '\0';
    last_wifi_ok     = !wifi_ok;

    wifi_btn_apply(wifi_ok);
  }

  if(habConfig == 1) {
    // --------- Service (dias restantes / postergado) ----------
    static char last_serviceStr[32] = "";

    char svcStr[32];
    time_t now = time(nullptr);

    g_service.formatConfigLabel(svcStr, sizeof(svcStr), now, net_time_valid());

    if(strcmp(svcStr, last_serviceStr) != 0) {
      strlcpy(last_serviceStr, svcStr, sizeof(last_serviceStr));
      lv_label_set_text(ui_lblOdoMaquina, svcStr);
    }

    if(strcmp(info_l, last_cfg_info) != 0) {
      strlcpy(last_cfg_info, info_l, sizeof(last_cfg_info));
      lv_label_set_text(ui_lblInfo, info_l);
    }

    const char *ip_show = wifi_ok ? ip_l : "0.0.0.0";
    if(strcmp(ip_show, last_cfg_ip) != 0) {
      strlcpy(last_cfg_ip, ip_show, sizeof(last_cfg_ip));
      lv_label_set_text(ui_lblNetworkIP, ip_show);
    }

    wifi_icons_apply(wifi_ok);

    if(cfg_scan_ready) {
      cfg_scan_ready = false;
      lv_dropdown_set_options(ui_Dropdown1, cfg_dd_opts);
      lv_dropdown_set_selected(ui_Dropdown1, 0);
    }

    lv_label_set_text(ui_lblMac, cfg_mac);
    lv_label_set_text(ui_lblFirmwareVersion, cfg_fw);

    bool busy = (cfg_wifi_state == CFG_WIFI_SCANNING) || (cfg_wifi_state == CFG_WIFI_CONNECTING);
    if(busy) lv_obj_add_state(ui_btnWifi, LV_STATE_DISABLED);
    else     lv_obj_clear_state(ui_btnWifi, LV_STATE_DISABLED);
  }

  // --------- iconos wifi ----------
  if(wifi_ok != last_wifi_ok) {
    last_wifi_ok = wifi_ok;
    wifi_icons_apply(wifi_ok);
    wifi_btn_apply(wifi_ok);
  }

  // --------- intervalado: series/pasadas ----------
  if(strcmp(s.intervalSeriesStr, last_intervalSeriesStr) != 0) {
    strlcpy(last_intervalSeriesStr, s.intervalSeriesStr, sizeof(last_intervalSeriesStr));
    lv_label_set_text(ui_lblNumSeries, s.intervalSeriesStr);
  }
  if(strcmp(s.intervalPasadasStr, last_intervalPasadasStr) != 0) {
    strlcpy(last_intervalPasadasStr, s.intervalPasadasStr, sizeof(last_intervalPasadasStr));
    lv_label_set_text(ui_lblNumPasadas, s.intervalPasadasStr);
  }
}
