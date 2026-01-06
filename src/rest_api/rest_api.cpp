#include <Arduino.h>
#include <WebServer.h>
#include "../app/app_globals.h"   // g_ui, g_ui_mutex, g_kcal_total_live, etc.
#include "rest_api.h"

static WebServer s_server(8080);
static bool s_running = false;

// CORS helper
static void send_cors() {
  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handle_options() {
  send_cors();
  s_server.send(204); // No Content
}

static void json_escape(const char* in, char* out, size_t out_sz) {
  if(!out || out_sz == 0) return;
  size_t j = 0;
  for(size_t i = 0; in && in[i] && j + 1 < out_sz; i++) {
    char c = in[i];
    if(c == '\"' || c == '\\') {
      if(j + 2 >= out_sz) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if((unsigned char)c < 0x20) {
      // evitar control chars en JSON
      if(j + 2 >= out_sz) break;
      out[j++] = '\\';
      out[j++] = ' ';
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

static void handle_telemetry() {
  // snapshot consistente de tu UI
  ui_state_t snap;
  xSemaphoreTake(g_ui_mutex, portMAX_DELAY);
  snap = g_ui;
  xSemaphoreGive(g_ui_mutex);

  int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

  char pace_esc[32];
  char ssid_esc[40];

  json_escape(snap.ritmoStr, pace_esc, sizeof(pace_esc));
  json_escape(cfg_ssid,      ssid_esc, sizeof(ssid_esc));
  // Armamos JSON: podés ajustar claves a tu dashboard
  // OJO: snap.distStr y snap.distUnit ya vienen formateados (mt/km)
  char json[750];
  snprintf(json, sizeof(json),
    "{"
      "\"speed_kmh\":%.1f,"
      "\"speed_str\":\"%s\","
      "\"pace_str\":\"%s\","
      "\"distance_str\":\"%s\","
      "\"distance_unit\":\"%s\","
      "\"calories_str\":\"%s\","
      "\"calories\":%.1f,"
      "\"train_time\":\"%s\","
      "\"session_active\":%s,"
      "\"wifi_ok\":%s,"
      "\"ssid\":\"%s\","
      "\"ip\":\"%s\","
      "\"rssi\":%d"
    "}",
    (double)snap.velocidad,
    snap.speedStr,
    pace_esc,
    snap.distStr,
    snap.distUnit,
    snap.calStr,
    (double)g_kcal_total_live,
    snap.trainTimeStr,
    sessionActive ? "true" : "false",
    wifi_ok ? "true" : "false",
    ssid_esc,
    cfg_ip,
    rssi
  );

  send_cors();
  s_server.send(200, "application/json", json);
}

void rest_api_start() {
  if (s_running) return;

  // Endpoints
  s_server.on("/api/telemetry", HTTP_OPTIONS, handle_options);
  s_server.on("/api/telemetry", HTTP_GET, handle_telemetry);

  // (opcional) ping
  s_server.on("/api/ping", HTTP_OPTIONS, handle_options);
  s_server.on("/api/ping", HTTP_GET, []() {
    send_cors();
    s_server.send(200, "application/json", "{\"ok\":true}");
  });

  s_server.begin();
  s_running = true;
}

void rest_api_stop() {
  if (!s_running) return;
  s_server.stop();
  s_running = false;
}

void rest_api_loop() {
  if (!s_running) return;
  s_server.handleClient();
}

bool rest_api_is_running() {
  return s_running;
}
