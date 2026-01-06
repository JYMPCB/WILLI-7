#pragma once
#include <Arduino.h>

// URL del manifest en GitHub Pages
#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL "https://TU_USUARIO.github.io/TU_REPO/ota/latest.json"
#endif

// Llamalo al conectar WiFi y/o cada X minutos
void ota_check_async();

// Lo llamás cuando el usuario toca "Actualizar"
void ota_start_async();

// Opcional: para que UI pueda “cancelar” antes de flashear (simple)
void ota_request_cancel();
