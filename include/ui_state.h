#pragma once
#include <stdbool.h>

// Estructura ÚNICA del estado de UI
typedef struct {

  // visibilidad
  bool headerHidden;
  bool settingsHidden;
  bool showReinitBtn;

  // hora / fecha
  bool showNetTime;
  char timeStr[8];
  char dateStr[11];

  // velocidad / ritmo
  float velocidad;      
  int cursorAngle;      
  char speedStr[16];   
  char ritmoStr[16];    

  // distancia
  char distStr[16];
  char distUnit[4];

  // calorías / tiempo
  char calStr[16];
  char trainTimeStr[16];

  // cartel de pausa
  bool pausaVisible;
  bool pausaIsSerie;
  char pausaTitle[24];
  char pausaStr[8];

  // intervalado
  char intervalSeriesStr[8];
  char intervalPasadasStr[8];
  char intervalSetpointStr[16];
  bool intervalShowSetpoint;    
  bool intervalIsDistance;
  bool forceBarsVisible;

    // ---- métricas RAW (para samples/gráfico) ----
  float dist_km;          // distancia total en km
  float speed_kmh;        // velocidad actual
  float pace_min_km;      // ritmo actual min/km
  float calories;         // calorías acumuladas
  uint32_t elapsed_ms;    // tiempo transcurrido

  // ---- estado intervalado RAW ----
  uint16_t interval_pass_idx;   // pasada actual (1..N)
  uint16_t interval_series_idx; // serie actual (1..N)

  // === Export REST / telemetría (numérico) ===
  float dist_m;           // distancia en metros (espacio_m)
  float kcal;             // kcal live
  uint32_t elapsed_s;     // tiempo total de sesión en segundos

  bool session_active;    // espejo de sessionActive
  bool workout_frozen;    // espejo de g_workout_frozen

} ui_state_t;
