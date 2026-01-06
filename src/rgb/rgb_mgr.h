#pragma once
#include <Arduino.h>
#include <lvgl.h>

struct Rgb8 { uint8_t r,g,b; };

void rgb_init_pwm(int pinR, int pinG, int pinB);

// setea el color objetivo (NO toca LVGL)
void rgb_set_target(Rgb8 c);

// tick: actualiza fade (PWM) y devuelve color actual ya “faded”
Rgb8 rgb_tick(uint8_t step = 8);

// helper: color según ritmo (configurable desde código)
Rgb8 pace_to_color(float ritmo);
