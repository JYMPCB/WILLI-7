#pragma once
#include <Arduino.h>

void rest_api_start();   // llama una vez cuando ya hay WiFi
void rest_api_stop();    // opcional
void rest_api_loop();
bool rest_api_is_running();