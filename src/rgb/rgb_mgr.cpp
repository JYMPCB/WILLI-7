#include "rgb_mgr.h"

// Canales LEDC
static const int CH_R = 3;
static const int CH_G = 4;
static const int CH_B = 5;

static int s_pinR = -1, s_pinG = -1, s_pinB = -1;

static Rgb8 s_curr = {0,0,0};
static Rgb8 s_tgt  = {0,0,0};

static inline uint8_t step_to(uint8_t cur, uint8_t tgt, uint8_t step){
  if(cur == tgt) return cur;
  if(cur < tgt){
    uint16_t v = cur + step;
    return (v > tgt) ? tgt : (uint8_t)v;
  }else{
    int16_t v = (int16_t)cur - step;
    return (v < tgt) ? tgt : (uint8_t)v;
  }
}

void rgb_init_pwm(int pinR, int pinG, int pinB){
  s_pinR = pinR; s_pinG = pinG; s_pinB = pinB;

  const int freq = 500;
  const int res  = 8;

  ledcSetup(CH_R, freq, res);
  ledcSetup(CH_G, freq, res);
  ledcSetup(CH_B, freq, res);

  ledcAttachPin(s_pinR, CH_R);
  ledcAttachPin(s_pinG, CH_G);
  ledcAttachPin(s_pinB, CH_B);

  ledcWrite(CH_R, 0);
  ledcWrite(CH_G, 0);
  ledcWrite(CH_B, 0);

  s_curr = {0,0,0};
  s_tgt  = {0,0,0};
}

void rgb_set_target(Rgb8 c){
  s_tgt = c;
}

Rgb8 rgb_tick(uint8_t step){
  
  Serial.printf("[%lu] ledcWrite RGB = %u %u %u\n",
  (unsigned long)millis(), s_curr.r, s_curr.g, s_curr.b);
  //s_curr = s_tgt;
  s_curr.r = step_to(s_curr.r, s_tgt.r, step);
  s_curr.g = step_to(s_curr.g, s_tgt.g, step);
  s_curr.b = step_to(s_curr.b, s_tgt.b, step);

  ledcWrite(CH_R, s_curr.r);
  ledcWrite(CH_G, s_curr.g);
  ledcWrite(CH_B, s_curr.b);

  return s_curr;
}

// ------------------- Mapeo ritmo->color (editable) -------------------

struct PaceColorBand { float min_pace, max_pace; Rgb8 color; };

static const PaceColorBand g_bands[] = {
  { 0.0f, 2.0f, {180,  0, 255} }, // LILA potente
  { 2.0f, 4.0f, {255,  0,   0} }, // ROJO
  { 4.0f, 5.0f, {255, 200,  0} }, // AMARILLO
  { 5.0f, 6.0f, {  0, 255,  0} }, // VERDE
  { 6.0f, 1e9f, {255, 255,255} }, // BLANCO
};

Rgb8 pace_to_color(float ritmo){
  if(ritmo <= 0.0f) return {0,0,0};
  for(const auto &b : g_bands){
    if(ritmo > b.min_pace && ritmo <= b.max_pace) return b.color;
  }
  return {0,0,0};
}
