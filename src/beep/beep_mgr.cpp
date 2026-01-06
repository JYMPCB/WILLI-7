#include "beep_mgr.h"

// Estado interno
static int      s_pin = BEEP_PIN;
static bool     s_inited = false;

static const uint16_t* s_freqs = nullptr;
static const uint16_t* s_durs  = nullptr;
static uint8_t  s_count = 0;
static uint8_t  s_idx = 0;

static uint32_t s_step_deadline = 0;
static bool     s_busy = false;
static uint8_t  s_duty = 128;

// Helpers: iniciar/detener tono (LEDC)
static void tone_start(uint16_t freq_hz, uint8_t duty) {
  if (freq_hz == 0) {
    ledcWrite(BEEP_LEDC_CH, 0);
    return;
  }
  ledcWriteTone(BEEP_LEDC_CH, freq_hz);
  ledcWrite(BEEP_LEDC_CH, duty);
}

static void tone_stop() {
  ledcWrite(BEEP_LEDC_CH, 0);
  // opcional: ledcWriteTone(ch, 0); no siempre es necesario
}

void beep_init(int pin) {
  s_pin = pin;

  // Setup LEDC y attach al pin
  ledcSetup(BEEP_LEDC_CH, 2000 /*dummy*/, BEEP_LEDC_RES);
  ledcAttachPin(s_pin, BEEP_LEDC_CH);

  tone_stop();
  s_inited = true;

  // reset estado patrón
  s_busy = false;
  s_freqs = nullptr;
  s_durs = nullptr;
  s_count = 0;
  s_idx = 0;
  s_step_deadline = 0;
}

void beep_stop() {
  if (!s_inited) return;
  tone_stop();
  s_busy = false;
  s_freqs = nullptr;
  s_durs = nullptr;
  s_count = 0;
  s_idx = 0;
  s_step_deadline = 0;
}

bool beep_is_busy() {
  return s_busy;
}

void beep_once(uint16_t freq_hz, uint16_t dur_ms, uint8_t duty) {
  static uint16_t f[2];
  static uint16_t d[2];
  f[0] = freq_hz; d[0] = dur_ms;
  f[1] = 0;       d[1] = 0;      // fin
  beep_play(f, d, 1, duty);
}

bool beep_play(const uint16_t* freqs_hz, const uint16_t* durs_ms, uint8_t count, uint8_t duty) {
  if (!s_inited) beep_init(s_pin);
  if (!freqs_hz || !durs_ms || count == 0) return false;

  // Si querés: permitir interrumpir siempre
  // beep_stop();

  s_freqs = freqs_hz;
  s_durs  = durs_ms;
  s_count = count;
  s_idx   = 0;
  s_busy  = true;
  s_duty  = duty;

  // arrancar primer paso ya
  uint16_t f0 = s_freqs[0];
  uint16_t t0 = s_durs[0];
  tone_start(f0, s_duty);
  s_step_deadline = millis() + t0;

  return true;
}

void beep_update() {
  if (!s_busy) return;
  uint32_t now = millis();

  // Ojo con overflow: usamos resta signed
  if ((int32_t)(now - s_step_deadline) < 0) return;

  // avanzar al siguiente paso
  s_idx++;
  if (s_idx >= s_count) {
    // fin
    beep_stop();
    return;
  }

  uint16_t f = s_freqs[s_idx];
  uint16_t t = s_durs[s_idx];

  tone_start(f, s_duty);
  s_step_deadline = now + t;
}
