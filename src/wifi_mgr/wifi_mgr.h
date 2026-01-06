#pragma once
#include <Arduino.h>

void wifiScanTask(void *pv);     // task entry
void wifi_mgr_loop();            // si querés mover el loop wifi acá
void wifi_mgr_on_enter_config();
void wifi_mgr_on_exit_config();