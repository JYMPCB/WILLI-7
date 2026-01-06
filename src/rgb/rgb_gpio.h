#pragma once
#include <Arduino.h>

// Inicialización (una sola vez)
void rgb_gpio_init_active_low(void);

// Set físico (activo LOW)
void rgb_gpio_set_active_low(bool r_on, bool g_on, bool b_on);

// 0..255 por canal, lógico (no invertido)
void rgb_set_target_u8(uint8_t r, uint8_t g, uint8_t b);

//Mapeo de color
void pace_to_rgb(float ritmo, uint8_t &r, uint8_t &g, uint8_t &b);

// Variables compartidas para sincronizar con LVGL flush
extern volatile uint8_t g_phy_r;
extern volatile uint8_t g_phy_g;
extern volatile uint8_t g_phy_b;
extern volatile bool    g_phy_pending;