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
  if(WiFi.status() == WL_CONNECTED) {
    start_sntp_if_needed();
    s_auto_done = true; // si ya conectó, no insistir más    
    return;
  }

  if(s_auto_done) return;

  if(s_auto_tries >= MAX_AUTO_TRIES) {
    s_auto_done = true; // rendirse, que lo hagan manual si quieren
    return;
  }

  uint32_t now = millis();
  if(now - s_reconn_t0 < RECONN_EVERY_MS) return;
  s_reconn_t0 = now;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();   // intenta con la última red guardada
  s_auto_tries++;
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
  cfg_wifi_state = CFG_WIFI_IDLE;

  if(cfg_server_on) {
    endwebserver();
    cfg_server_on = false;
  }
}

void wifi_mgr_loop()
{
  static bool s_ota_checked_this_boot = false;
  // ✅ mantener globals coherentes siempre
  wifi_sync_globals_from_status();  

  wl_status_t st = WiFi.status();

  // detectar transición (conecta/reconecta)
  if (st == WL_CONNECTED && s_last_st != WL_CONNECTED) {
    rest_api_start();   // ✅ solo una vez al conectar/reconectar

    // Chequear OTA una vez por boot (o por reconexión)
    if (!s_ota_checked_this_boot && !g_ota_active) {
      s_ota_checked_this_boot = true;
      ota_check_async();
    }    
  }

  // detectar desconexión
  if (st != WL_CONNECTED && s_last_st == WL_CONNECTED) {
    rest_api_stop();
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

    // NO desconectar si ya está conectado (si desconectás, rompés icono/IP/OTA)
    WiFi.mode(WIFI_STA);

    // limpiar resultado anterior y arrancar scan async
    WiFi.scanDelete();
    delay(50); // opcional, cortito

    WiFi.scanNetworks(true, true);  // async scan, show hidden = true
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
