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

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

/* Canonical STA MAC: AA:BB:CC:DD:EE:FF (uppercase). Ignores :, -, . */
static bool mac_canon(const char* in, char* out, size_t out_len) {
  if (!in || !out || out_len < 18) return false;
  char hex[12];
  int n = 0;
  for (const char* p = in; *p && n < 12; ++p) {
    int h = hex_nibble(*p);
    if (h >= 0) hex[n++] = (char)(h < 10 ? ('0' + h) : ('A' + (h - 10)));
  }
  if (n != 12) {
    out[0] = '\0';
    return false;
  }
  snprintf(out, out_len, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           hex[0], hex[1], hex[2], hex[3], hex[4], hex[5],
           hex[6], hex[7], hex[8], hex[9], hex[10], hex[11]);
  return true;
}

static String http_get_body(const char* url) {
  String empty;
  if (!url || !url[0]) return empty;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(OTA_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return empty;
  int code = http.GET();
  String body;
  if (code == 200) body = http.getString();
  http.end();
  return body;
}

static bool parse_manifest(const String& body, char* ver, size_t ver_len,
                           char* bin, size_t bin_len, char* notes, size_t notes_len) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  const char* latest_ver = doc["version"] | "";
  const char* bin_url    = doc["bin_url"]  | "";
  const char* n          = doc["notes"]    | "";
  if (strlen(latest_ver) == 0 || strlen(bin_url) == 0) return false;
  strlcpy(ver, latest_ver, ver_len);
  strlcpy(bin, bin_url, bin_len);
  strlcpy(notes, n, notes_len);
  return true;
}

static bool mac_is_allowed(const String& allow_body) {
  JsonDocument doc;
  if (deserializeJson(doc, allow_body)) return false;
  if (doc["all"] | false) return true;
  char local[18];
  String raw = WiFi.macAddress();
  if (!mac_canon(raw.c_str(), local, sizeof(local))) return false;
  JsonArray arr = doc["macs"].as<JsonArray>();
  if (arr.isNull()) return false;
  for (JsonVariant v : arr) {
    const char* s = v.as<const char*>();
    char cand[18];
    if (s && mac_canon(s, cand, sizeof(cand)) && strcmp(local, cand) == 0) {
      return true;
    }
  }
  return false;
}

static void commit_offer(const char* ver, const char* bin, const char* notes) {
  strlcpy(g_ota_latest_ver, ver, sizeof(g_ota_latest_ver));
  strlcpy(g_ota_bin_url, bin, sizeof(g_ota_bin_url));
  strlcpy(g_ota_notes, notes, sizeof(g_ota_notes));
  g_ota_available = true;
  set_status("Update disponible");
}

// ---------- OTA CHECK TASK ----------
static void ota_check_task(void* pv) {
  (void)pv;

  if (g_ota_check_running) { vTaskDelete(NULL); return; }
  g_ota_check_running = true;

  char ver[16] = {0};
  char bin[256] = {0};
  char notes[256] = {0};
  String latest;
  String next;
  String allow;

  if (WiFi.status() != WL_CONNECTED) {
    set_status("Sin WiFi");
    goto done;
  }

  set_status("Buscando update...");
  g_ota_available = false;

  latest = http_get_body(OTA_MANIFEST_URL);
  if (latest.length() == 0) {
    set_status("Manifest HTTP error");
    goto done;
  }
  if (!parse_manifest(latest, ver, sizeof(ver), bin, sizeof(bin), notes, sizeof(notes))) {
    set_status("JSON invalido");
    goto done;
  }

  /* Canal A: 1.0.11 → 1.0.12. Sin MAC. latest.json nunca pasa de 1.0.12. */
  if (semver_cmp(ver, g_fw_version) > 0) {
    commit_offer(ver, bin, notes);
    goto done;
  }

  next = http_get_body(OTA_NEXT_URL);
  if (next.length() == 0) {
    set_status("Al dia");
    goto done;
  }
  if (!parse_manifest(next, ver, sizeof(ver), bin, sizeof(bin), notes, sizeof(notes))) {
    set_status("Al dia");
    goto done;
  }
  if (semver_cmp(ver, g_fw_version) <= 0) {
    set_status("Al dia");
    goto done;
  }

  allow = http_get_body(OTA_ALLOW_URL);
  if (allow.length() == 0 || !mac_is_allowed(allow)) {
    set_status("No habilitada");
    goto done;
  }

  commit_offer(ver, bin, notes);

done:
  g_ota_check_running = false;
  vTaskDelete(NULL);
}

void ota_check_async() {
  // Task liviana, no bloquea UI
  xTaskCreatePinnedToCore(ota_check_task, "ota_check", 10240, NULL, 1, NULL, 0);
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
