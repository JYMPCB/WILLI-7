#include "ota_mgr.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "app/app_globals.h"

// ---------- Config ----------
static const uint32_t OTA_HTTP_TIMEOUT_MS = 15000;
static volatile bool s_cancel_req = false;

static int semver_cmp(const char* a, const char* b) {
  // Compara "MAJOR.MINOR.PATCH" (simple y suficiente)
  int a1=0,a2=0,a3=0,b1=0,b2=0,b3=0;
  sscanf(a ? a : "0.0.0", "%d.%d.%d", &a1,&a2,&a3);
  sscanf(b ? b : "0.0.0", "%d.%d.%d", &b1,&b2,&b3);
  if(a1!=b1) return (a1>b1)? 1:-1;
  if(a2!=b2) return (a2>b2)? 1:-1;
  if(a3!=b3) return (a3>b3)? 1:-1;
  return 0;
}

static void set_status(const char* s) {
  if(!s) s = "";
  strlcpy(g_ota_status, s, sizeof(g_ota_status));
}

void ota_request_cancel() {
  s_cancel_req = true;
}

// ---------- OTA CHECK TASK ----------
static void ota_check_task(void* pv) {
  (void)pv;

  if (g_ota_check_running) { vTaskDelete(NULL); return; }
  g_ota_check_running = true;

  if (WiFi.status() != WL_CONNECTED) {
    set_status("Sin WiFi");
    vTaskDelete(NULL);
    return;
  }

  set_status("Buscando update...");
  g_ota_available = false;

  WiFiClientSecure client;
  client.setInsecure(); // Simple (sin CA). Luego lo endurecemos.
  HTTPClient http;

  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  if (!http.begin(client, OTA_MANIFEST_URL)) {
    set_status("Error begin manifest");
    vTaskDelete(NULL);
    return;
  }

  int code = http.GET();
  if (code != 200) {
    set_status("Manifest HTTP error");
    http.end();
    vTaskDelete(NULL);
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    set_status("JSON invalido");
    vTaskDelete(NULL);
    return;
  }

  const char* latest_ver = doc["version"] | "";
  const char* bin_url    = doc["bin_url"]  | "";
  const char* notes      = doc["notes"]    | "";

  if (strlen(latest_ver) == 0 || strlen(bin_url) == 0) {
    set_status("Manifest incompleto");
    vTaskDelete(NULL);
    return;
  }

  // Comparo con tu versión local
  if (semver_cmp(latest_ver, g_fw_version) > 0) {
    strlcpy(g_ota_latest_ver, latest_ver, sizeof(g_ota_latest_ver));
    strlcpy(g_ota_bin_url, bin_url, sizeof(g_ota_bin_url));
    strlcpy(g_ota_notes, notes, sizeof(g_ota_notes));
    g_ota_available = true;
    set_status("Update disponible");
  } else {
    set_status("Al dia");
  }

  vTaskDelete(NULL);
}

void ota_check_async() {
  // Task liviana, no bloquea UI
  xTaskCreatePinnedToCore(ota_check_task, "ota_check", 8192, NULL, 1, NULL, 0);
}

// ---------- OTA START TASK ----------
static void ota_start_task(void* pv) {
  (void)pv;

  if (WiFi.status() != WL_CONNECTED) {
    set_status("Sin WiFi");
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  if (!g_ota_available || strlen(g_ota_bin_url) == 0) {
    set_status("No hay update");
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  s_cancel_req = false;
  g_ota_progress = 0;
  g_ota_active = true;
  set_status("Descargando...");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.setTimeout(OTA_HTTP_TIMEOUT_MS);

  if (!http.begin(client, g_ota_bin_url)) {
    set_status("Error begin BIN");
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  int code = http.GET();
  if (code != 200) {
    set_status("HTTP BIN error");
    http.end();
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  int totalLen = http.getSize();               // puede ser -1
  WiFiClient* stream = http.getStreamPtr();

  if (s_cancel_req) {
    set_status("Cancelado");
    http.end();
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  // Callback de progreso (si tu core lo soporta, casi siempre sí)
  Update.onProgress([](size_t done, size_t total) {
    if (total > 0) {
      int p = (int)((done * 100UL) / total);
      if (p < 0) p = 0;
      if (p > 100) p = 100;
      g_ota_progress = p;
    }
  });

  if (!Update.begin((totalLen > 0) ? (size_t)totalLen : UPDATE_SIZE_UNKNOWN)) {
    set_status("Update.begin fail");
    http.end();
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  set_status("Cargando...");

  // Escribe por stream (no ocupa RAM)
  size_t written = Update.writeStream(*stream);

  if (s_cancel_req) {
    // Ojo: si ya se empezó a escribir, cancelar no garantiza estado consistente.
    // Por eso lo dejamos solo como “best effort”.
    set_status("Cancelado");
    Update.abort();
    http.end();
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  bool okEnd = Update.end(true);
  http.end();

  if (!okEnd || Update.hasError()) {
    set_status("OTA fallo");
    g_ota_active = false;
    vTaskDelete(NULL);
    return;
  }

  g_ota_progress = 100;
  set_status("OK, reiniciando");

  delay(600);
  ESP.restart();
}

void ota_start_async() {
  if (g_ota_active) return;
  xTaskCreatePinnedToCore(ota_start_task, "ota_start", 12288, NULL, 2, NULL, 0);
}
