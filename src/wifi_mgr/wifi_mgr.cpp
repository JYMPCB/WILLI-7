#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "../app/app_globals.h"
#include "wifi_mgr.h"
#include "dataWilli.h"
#include "rest_api/rest_api.h"
#include "../ota/ota_mgr.h"

//rest api
//static bool s_rest_up = false;     // para disparar 1 sola vez por conexión
static wl_status_t s_last_st = WL_IDLE_STATUS;

// OTA (los tenés en main)
extern void initwebserver();
extern void handle_server();
extern void endwebserver();

static bool cfg_server_on = false;

// conexión no bloqueante
static uint32_t connect_t0 = 0;
static const uint32_t CONNECT_TIMEOUT_MS = 15000;

// scan async
static uint32_t scan_t0 = 0;
static const uint32_t SCAN_TIMEOUT_MS = 15000;

// --- mantenimiento fuera de config ---
static uint32_t s_reconn_t0 = 0;
static const uint32_t RECONN_EVERY_MS = 5000;
static uint8_t  s_auto_tries = 0;
static const uint8_t MAX_AUTO_TRIES = 3;
static bool     s_auto_done = false;

// SNTP
static bool s_sntp_started = false;

static void start_sntp_if_needed()
{
  if(s_sntp_started) return;
  s_sntp_started = true;

  // Argentina UTC-3 (sin DST)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

// ✅ ÚNICA fuente de verdad hacia tus globals
static void wifi_sync_globals_from_status()
{
  wl_status_t st = WiFi.status();

  if(st == WL_CONNECTED) {
    wifi_ok = true;
    strlcpy(cfg_ssid, WiFi.SSID().c_str(), sizeof(cfg_ssid));
    strlcpy(cfg_ip,   WiFi.localIP().toString().c_str(), sizeof(cfg_ip));
    start_sntp_if_needed();
  } else {
    wifi_ok = false;
    strlcpy(cfg_ssid, "SIN RED", sizeof(cfg_ssid));
    strlcpy(cfg_ip,   "0.0.0.0", sizeof(cfg_ip));
  }
}

// Reconexión limitada fuera de config (no rompe scan)
static void wifi_maintenance()
{
  wl_status_t st = WiFi.status();

  if(st == WL_CONNECTED) {
    start_sntp_if_needed();
    // si conectó, reseteo contadores por prolijidad
    s_auto_tries = 0;
    s_reconn_t0  = millis();
    return;
  }

  uint32_t now = millis();
  if(now - s_reconn_t0 < RECONN_EVERY_MS) return;
  s_reconn_t0 = now;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Intento suave: usa credenciales guardadas por el stack
  WiFi.reconnect();

  // (opcional) solo para diagnóstico/telemetría
  if(s_auto_tries < 250) s_auto_tries++;
}

// helpers
static void build_dropdown_opts_from_scan(int n)
{
  cfg_dd_opts[0] = '\0';
  scan_n = 0;

  int count = (n > MAX_NETS) ? MAX_NETS : n;
  scan_n = count;

  for(int i = 0; i < count; i++) {
    String s = WiFi.SSID(i);

    // sanitizar
    s.replace("\n", " ");
    s.trim();

    strlcpy(scan_ssid[i], s.c_str(), sizeof(scan_ssid[i]));

    // armar opciones "SSID1\nSSID2\n..."
    if(s.length() > 0) {
      if(strlen(cfg_dd_opts) + s.length() + 2 < sizeof(cfg_dd_opts)) {
        strlcat(cfg_dd_opts, s.c_str(), sizeof(cfg_dd_opts));
        strlcat(cfg_dd_opts, "\n", sizeof(cfg_dd_opts));
      }
    }
  }

  if(scan_n <= 0 || cfg_dd_opts[0] == '\0') {
    strlcpy(cfg_dd_opts, "SIN REDES", sizeof(cfg_dd_opts));
  }
}

void wifi_mgr_on_enter_config()
{
  // Datos estáticos/diagnóstico
  strlcpy(cfg_mac, WiFi.macAddress().c_str(), sizeof(cfg_mac));
  strlcpy(cfg_fw, g_fw_version ? g_fw_version : "?", sizeof(cfg_fw));

  // Mensaje inicial
  if(wifi_ok) snprintf(cfg_info, sizeof(cfg_info), "Conectado a %s", cfg_ssid);
  else        snprintf(cfg_info, sizeof(cfg_info), "Desconectado");

  // No tocar OTA acá: se controla en el bloque OTA al final del loop
}

void wifi_mgr_on_exit_config()
{
  WiFi.setAutoReconnect(true);
  cfg_wifi_state = CFG_WIFI_IDLE;

  if(cfg_server_on) {
    endwebserver();
    cfg_server_on = false;
  }
}

void wifi_mgr_loop()
{
  static uint32_t s_ota_next_ms = 0;
  static const uint32_t OTA_FIRST_DELAY_MS = 8000;           // 8s post-conexión
  static const uint32_t OTA_PERIOD_MS      = 5UL*60UL*1000UL;

  static uint32_t t = 0;
if(millis() - t > 1000) {
  t = millis();
  Serial.printf("[wifi] st=%d habConfig=%d tries=%u ssid='%s' ip='%s'\n",
                (int)WiFi.status(), habConfig, (unsigned)s_auto_tries, cfg_ssid, cfg_ip);
}

  static bool s_ota_checked_this_boot = false;
  // ✅ mantener globals coherentes siempre
  wifi_sync_globals_from_status();  

  wl_status_t st = WiFi.status();

  // detectar transición (conecta/reconecta)
  if (st == WL_CONNECTED && s_last_st != WL_CONNECTED) {
    rest_api_start();   // ✅ solo una vez al conectar/reconectar

    // programar primer check OTA (no inmediato)
    s_ota_next_ms = millis() + OTA_FIRST_DELAY_MS;   
  }

  // detectar desconexión
  if (st != WL_CONNECTED && s_last_st == WL_CONNECTED) {
    rest_api_stop();

    // ✅ permitir reconexión automática otra vez
    s_auto_done  = false;
    s_auto_tries = 0;
    s_reconn_t0  = 0;

    // (opcional) permitir nuevo check OTA cuando reconecte
    s_ota_checked_this_boot = false;
  }

  s_last_st = st;

  // mantener el server vivo (si está corriendo)
  if (st == WL_CONNECTED) {
    rest_api_loop();
  }


  // Entró a config (1 vez)
  if(cfg_entered) {
    cfg_entered = false;
    wifi_mgr_on_enter_config();

    if(WiFi.status() == WL_CONNECTED) {
      snprintf(cfg_info, sizeof(cfg_info), "Conectado a %s", cfg_ssid);
      cfg_wifi_state = CFG_WIFI_CONNECTED;
    } else {
      snprintf(cfg_info, sizeof(cfg_info), "Desconectado");
      cfg_wifi_state = CFG_WIFI_IDLE;
    }
  }

  // ------------------- OTA auto-check (solo con WiFi conectado y estable) -------------------
  if (st == WL_CONNECTED && !g_ota_active && s_ota_next_ms != 0) {
    uint32_t now = millis();
    if ((int32_t)(now - s_ota_next_ms) >= 0 && !g_ota_check_running) {
      ota_check_async();
      s_ota_next_ms = now + OTA_PERIOD_MS;   // cada 5 min
    }
  }

  // Si NO estás en config: mantenimiento + apagar OTA si estaba
  if(habConfig != 1) {
    wifi_maintenance();
    if(cfg_server_on) wifi_mgr_on_exit_config();
    return;
  }

  // ------------------- DISCONNECT (manual) -------------------
  if(cfg_do_disconnect) {
    cfg_do_disconnect = false;

    WiFi.scanDelete();
    WiFi.disconnect(true, false);   // ✅ NO borrar credenciales

    snprintf(cfg_info, sizeof(cfg_info), "Desconectado");
    cfg_wifi_state = CFG_WIFI_IDLE;
    // (wifi_ok/cfg_ssid/cfg_ip los fija wifi_sync... arriba)
  }

  // ------------------- START SCAN -------------------
  if(cfg_need_scan) {
    cfg_need_scan = false;

    cfg_wifi_state = CFG_WIFI_SCANNING;
    snprintf(cfg_info, sizeof(cfg_info), "Buscando redes...");

    WiFi.mode(WIFI_STA);

    // ✅ Si YA estoy conectado, NO desconectar (si no, se cae el icono/IP)
    // ✅ Solo “limpiar” cuando estoy desconectado y el stack está intentando autenticarse
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.setAutoReconnect(false);      // frena loops de reconexión durante el scan
      WiFi.disconnect(false, false);     // no borrar credenciales
      delay(50);
    } else {
      WiFi.setAutoReconnect(true);       // mantener conexión viva
    }

    WiFi.scanDelete();
    delay(50);

    WiFi.scanNetworks(true, true);       // async scan
    scan_t0 = millis();

    cfg_scan_ready = false;
    cfg_dd_opts_ready = false;
    scan_n = 0;
    cfg_dd_opts[0] = '\0';
  }

  // ------------------- SCAN PROGRESS -------------------
  if(cfg_wifi_state == CFG_WIFI_SCANNING) {
    int n = WiFi.scanComplete();

    if(n == WIFI_SCAN_RUNNING) {
      // nada
    }
    else if(n >= 0) {
      build_dropdown_opts_from_scan(n);

      if(scan_n > 0) snprintf(cfg_info, sizeof(cfg_info), "Redes: %d", scan_n);
      else           snprintf(cfg_info, sizeof(cfg_info), "No hay redes disponibles");

      cfg_scan_ready = true;
      cfg_dd_opts_ready = true;

      WiFi.scanDelete();
      WiFi.setAutoReconnect(true);
      cfg_wifi_state = CFG_WIFI_SCAN_DONE;
      
    }
    else { // WIFI_SCAN_FAILED
      if(millis() - scan_t0 > SCAN_TIMEOUT_MS) {
        strlcpy(cfg_dd_opts, "SIN REDES", sizeof(cfg_dd_opts));
        scan_n = 0;

        snprintf(cfg_info, sizeof(cfg_info), "Error/timeout de scan");
        cfg_scan_ready = true;
        cfg_dd_opts_ready = true;

        WiFi.scanDelete();
        WiFi.setAutoReconnect(true);
        cfg_wifi_state = CFG_WIFI_FAIL;
      }
    }
  }

  // ------------------- CONNECT START -------------------
  if(cfg_do_connect) {
    cfg_do_connect = false;

    WiFi.scanDelete();

    if(strlen(pending_ssid) == 0 || strcmp(pending_ssid, "SIN REDES") == 0) {
      snprintf(cfg_info, sizeof(cfg_info), "Selecciona una red");
    } else {
      snprintf(cfg_info, sizeof(cfg_info), "Conectando a %s...", pending_ssid);

      WiFi.mode(WIFI_STA);
      WiFi.disconnect(true, false);  // ✅ NO borrar credenciales
      delay(50);

      WiFi.begin(pending_ssid, pending_pass);

      cfg_wifi_state = CFG_WIFI_CONNECTING;
      connect_t0 = millis();
    }
  }

  // ------------------- CONNECT PROGRESS -------------------
  if(cfg_wifi_state == CFG_WIFI_CONNECTING) {
    wl_status_t st = WiFi.status();

    if(st == WL_CONNECTED) {
      snprintf(cfg_info, sizeof(cfg_info), "Conectado a %s", WiFi.SSID().c_str());
      cfg_wifi_state = CFG_WIFI_CONNECTED;

      // habilitar NTP
      start_sntp_if_needed();

      // al conectar manualmente, también damos por "done" el auto-connect
      s_auto_done = true;
    } else {
      uint32_t elapsed = millis() - connect_t0;

      if(elapsed > CONNECT_TIMEOUT_MS) {
        snprintf(cfg_info, sizeof(cfg_info), "Fallo al conectar (timeout)");
        WiFi.disconnect(true, false);   // ✅ NO borrar credenciales
        cfg_wifi_state = CFG_WIFI_FAIL;
      } else {
        if(st == WL_NO_SSID_AVAIL)        snprintf(cfg_info, sizeof(cfg_info), "Red no disponible");
        else if(st == WL_CONNECT_FAILED)  snprintf(cfg_info, sizeof(cfg_info), "Clave incorrecta?");
        else if(st == WL_CONNECTION_LOST) snprintf(cfg_info, sizeof(cfg_info), "Conexion perdida...");
        else                             snprintf(cfg_info, sizeof(cfg_info), "Conectando... (%lus)", (unsigned long)(elapsed/1000));
      }
    }
  }

  // ------------------- OTA (SOLO EN CONFIG) -------------------
  if(WiFi.status() == WL_CONNECTED && !g_ota_active) {
    if(!cfg_server_on) {
      initwebserver();
      cfg_server_on = true;
    }
    handle_server();
  } else {
    if(cfg_server_on) {
      endwebserver();
      cfg_server_on = false;
    }
  }
}
