#pragma once
#include <Arduino.h>

// -------------------- Config --------------------
#ifndef BEEP_PIN
#define BEEP_PIN 17
#endif

// Elegí un canal que no uses para otra cosa (0..7 en ESP32 Arduino normalmente).
#ifndef BEEP_LEDC_CH
#define BEEP_LEDC_CH 14
#endif

#ifndef BEEP_LEDC_RES
#define BEEP_LEDC_RES 8   // 8 bits es suficiente para un "beep" (duty 0..255)
#endif

// -------------------- API --------------------
// Inicializa el pin/canal LEDC (llamar una vez en setup/app_init)
void beep_init(int pin = BEEP_PIN);

// Lanza un beep simple (no bloqueante). duty 0..255 (ej 128)
void beep_once(uint16_t freq_hz, uint16_t dur_ms, uint8_t duty = 128);

// Lanza un patrón (no bloqueante) con arrays de pasos:
// - freqs_hz[i] = frecuencia (0 = silencio)
// - durs_ms[i]  = duración de ese paso
// count = cantidad de pasos
bool beep_play(const uint16_t* freqs_hz, const uint16_t* durs_ms, uint8_t count, uint8_t duty = 128);

// Debe llamarse seguido (ej: en tu ui_refresh o loop) para avanzar el patrón
void beep_update();

// Para cortar cualquier sonido/patrón
void beep_stop();

// Devuelve true si hay un patrón en curso
bool beep_is_busy();
