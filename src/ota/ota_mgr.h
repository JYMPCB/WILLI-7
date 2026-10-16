#pragma once
#include <Arduino.h>

// Canal A: 3.x y 1.0.11 solo leen esto. Se queda en 4.0.0 Arduino para siempre.
#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL "https://jympcb.github.io/WILLI-7/ota/latest.json"
#endif
// Canal B: el puente 4.0.0 lee next.json + allow.json. 3.x y 1.0.11 no lo consultan.
#ifndef OTA_NEXT_URL
#define OTA_NEXT_URL "https://jympcb.github.io/WILLI-7/ota/next.json"
#endif
#ifndef OTA_ALLOW_URL
#define OTA_ALLOW_URL "https://jympcb.github.io/WILLI-7/ota/allow.json"
#endif

// Llamalo al conectar WiFi y/o cada X minutos
void ota_check_async();

// Lo llamás cuando el usuario toca "Actualizar"
void ota_start_async();

// Opcional: para que UI pueda “cancelar” antes de flashear (simple)
void ota_request_cancel();
