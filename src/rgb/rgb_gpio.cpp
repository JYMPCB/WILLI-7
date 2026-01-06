#include "rgb_gpio.h"
#include "app/app_globals.h"

#define redOut   11
#define greenOut 12
#define blueOut  13

volatile uint8_t g_phy_r = 0;
volatile uint8_t g_phy_g = 0;
volatile uint8_t g_phy_b = 0;
volatile bool    g_phy_pending = false;

void rgb_gpio_init_active_low(void)
{
  pinMode(redOut,   OUTPUT);
  pinMode(greenOut, OUTPUT);
  pinMode(blueOut,  OUTPUT);

  // OFF inicial (activo LOW)
  digitalWrite(redOut,   HIGH);
  digitalWrite(greenOut, HIGH);
  digitalWrite(blueOut,  HIGH);
}

void rgb_gpio_set_active_low(bool r_on, bool g_on, bool b_on)
{
  digitalWrite(redOut,   r_on ? LOW : HIGH);
  digitalWrite(greenOut, g_on ? LOW : HIGH);
  digitalWrite(blueOut,  b_on ? LOW : HIGH);
}

void rgb_set_target_u8(uint8_t r, uint8_t g, uint8_t b){
  // Guardamos como booleano ON/OFF o como 8-bit según quieras.
  // Por ahora ON/OFF (sin PWM): ON si >0
  g_phy_r = (r > 0) ? 1 : 0;
  g_phy_g = (g > 0) ? 1 : 0;
  g_phy_b = (b > 0) ? 1 : 0;
  g_phy_pending = true;
  g_rgb_target_r = r;
  g_rgb_target_g = g;
  g_rgb_target_b = b;
}

void pace_to_rgb(float ritmo, uint8_t &r, uint8_t &g, uint8_t &b){
  // ritmo en min/km (según tu lógica actual)
  if(ritmo <= 0) { r=g=b=0; return; }

  if(ritmo > 0 && ritmo <= 2)      { r=180; g=0;   b=255; return; } // lila potente
  else if(ritmo > 2 && ritmo <= 4) { r=255; g=0;   b=0;   return; } // rojo
  else if(ritmo > 4 && ritmo <= 5) { r=255; g=200; b=0;   return; } // amarillo
  else if(ritmo > 5 && ritmo <= 6) { r=0;   g=255; b=0;   return; } // verde
  else                              { r=255; g=255; b=255; return; } // blanco
}