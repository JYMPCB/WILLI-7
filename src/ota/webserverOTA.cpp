#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "data.h"
#include "ota_state.h"


WebServer server(80);

// -------------------- estado OTA --------------------
static volatile bool g_reboot_pending = false;
static uint32_t g_reboot_at_ms = 0;

static bool g_ota_ok = false;
static String g_ota_err;

// Uptime simple
static uint32_t boot_ms = 0;

static void schedule_reboot(uint32_t delay_ms = 700) {
  g_reboot_pending = true;
  g_reboot_at_ms = millis() + delay_ms;
}

// -------------------- helpers --------------------
static void send_no_cache_headers() {
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

static String json_escape(const String& s) {
  String out; out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default: out += c; break;
    }
  }
  return out;
}

// -------------------- páginas --------------------
void PaginaSimple() {
  send_no_cache_headers();
  server.send(200, "text/html; charset=utf-8", Pagina);
}

// Opcional: info para mostrar en la web
void Info() {
  send_no_cache_headers();

  // Si querés una "versión", definila en build flags o en un header
  // #define FW_VERSION "1.2.3"
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

  uint32_t up_s = (millis() - boot_ms) / 1000;
  uint32_t up_m = up_s / 60;
  uint32_t up_h = up_m / 60;

  String ip = WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
  uint32_t heap = ESP.getFreeHeap();

  String json = "{";
  json += "\"fw\":\"" + String(FW_VERSION) + "\",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"heap\":\"" + String(heap) + "\",";
  json += "\"uptime\":\"" + String(up_h) + "h " + String(up_m % 60) + "m " + String(up_s % 60) + "s\"";
  json += "}";

  server.send(200, "application/json; charset=utf-8", json);
}

// -------------------- OTA --------------------
void ActualizarPaso1() {
  // Este handler se llama al final del POST, después de ActualizarPaso2
  send_no_cache_headers();

  // Devolvemos JSON (igual si tu frontend no lo parsea, sirve)
  String body;
  if (g_ota_ok && !Update.hasError()) {
    body = "{\"ok\":true,\"msg\":\"Actualizado con éxito. Reiniciando...\"}";
    server.send(200, "application/json; charset=utf-8", body);
    schedule_reboot(700);
  } else {
    String err = g_ota_err;
    if (err.length() == 0) err = "Falla al actualizar";
    body = "{\"ok\":false,\"msg\":\"" + json_escape(err) + "\"}";
    server.send(500, "application/json; charset=utf-8", body);
    // NO reiniciamos en error
  }
}

void ActualizarPaso2() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_ota_active = true;
    Serial.println("OTA START");
    g_ota_ok = false;
    g_ota_err = "";

    Serial.setDebugOutput(false); // mejor no spamear, si querés true lo activás
    Serial.printf("OTA start: %s, size(unknown yet)\n", upload.filename.c_str());

    // Consejo: si querés forzar que sea firmware, podés validar nombre .bin
    // (no es seguridad real, pero evita errores de usuario)
    if (!upload.filename.endsWith(".bin")) {
      g_ota_err = "El archivo no parece .bin";
      // Igual seguimos leyendo para consumir el POST, pero no iniciamos Update
      return;
    }

    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Update.begin() falló. Error=%d", Update.getError());
      g_ota_err = buf;
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_ota_err.length()) return; // si ya falló, no seguir escribiendo

    size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Update.write() incompleto (%u/%u). Error=%d",
               (unsigned)written, (unsigned)upload.currentSize, Update.getError());
      g_ota_err = buf;
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    g_ota_active = false;
    Serial.println("OTA END");
    if (g_ota_err.length()) return;

    if (Update.end(true)) {
      g_ota_ok = true;
      Serial.printf("OTA success: %u bytes\n", (unsigned)upload.totalSize);
      
    } else {
      char buf[128];
      snprintf(buf, sizeof(buf), "Update.end() falló. Error=%d", Update.getError());
      g_ota_err = buf;
      Update.printError(Serial);
    }
    
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    g_ota_active = false;
    Serial.println("OTA ABORTED");
    g_ota_err = "Carga abortada (UPLOAD_FILE_ABORTED)";  
    
  } else {
    // otros estados raros
    Serial.printf("OTA status=%d\n", upload.status);
  }

  yield();
}

// -------------------- init/loop --------------------
void initwebserver() {
  if (boot_ms == 0) boot_ms = millis();

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    server.on("/", HTTP_GET, PaginaSimple);
    server.on("/info", HTTP_GET, Info); // opcional
    server.on("/actualizar", HTTP_POST, ActualizarPaso1, ActualizarPaso2);

    server.begin();

    Serial.print("OTA web en: http://");
    Serial.println(WiFi.localIP());
  }
}

void handle_server() {
  server.handleClient();

  if (g_reboot_pending && (int32_t)(millis() - g_reboot_at_ms) >= 0) {
    g_reboot_pending = false;
    delay(50);
    ESP.restart();
  }
}

void endwebserver() {
  server.stop();
}
