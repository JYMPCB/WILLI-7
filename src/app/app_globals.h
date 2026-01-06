#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/portmacro.h"
#include <stddef.h>
#include "ui_state.h"
#include <stdint.h>
#include <stdbool.h>
#include "service/service_mgr.h"


// --- Firmware version ---
extern const char* g_fw_version;     // "1.3.2"
extern const uint32_t g_fw_build;    // 20260106 or epoch

// --- OTA state ---
extern volatile bool g_ota_available;
extern volatile bool g_ota_active;
extern volatile int  g_ota_progress;     // 0..100

extern char g_ota_latest_ver[16];
extern char g_ota_bin_url[256];
extern char g_ota_notes[256];
extern char g_ota_status[64];          // texto corto de estado

// ---------------- UI request flags (solo marcas) ----------------
extern volatile uint32_t g_ui_req_flags;

enum {
  UI_REQ_PROG_REFRESH       = (1u << 0),
  UI_REQ_PROG_MODE_REFRESH  = (1u << 1),
  UI_REQ_WIFI_TOGGLE        = (1u << 2),
  UI_REQ_WIFI_SCAN          = (1u << 3),
  UI_REQ_SERVICE_ACCEPT     = (1u << 18),
  UI_REQ_SERVICE_POSTPONE   = (1u << 19),
};

//service
extern ServiceMgr g_service;

//------------ usadas en mathTask
extern size_t tiempoHour;
extern size_t tiempoMin;
extern size_t tiempoSec;
extern float  espacio;
extern size_t pausaMacro;
extern size_t pausaMicro;
extern size_t fin;
extern size_t numSeriesMath;
extern size_t numPasadasMath;
extern bool habLocalTime;
extern float calorias;
extern float horasCinta;
extern bool habChart;

//------------ usadas en speedTask
#define tita  6.28   // 2*PI (hay un solo sensor)
#define radio 128  // 128mm
extern float drdisco;

//metricas para medicion y testing
extern volatile uint32_t g_flush_count;
extern volatile uint32_t g_flush_worst_us;
extern volatile uint32_t g_flush_last_us;
extern volatile int32_t  g_flush_worst_w;
extern volatile int32_t  g_flush_worst_h;
extern volatile uint32_t g_touch_last_us;
extern volatile uint32_t g_touch_worst_us;
extern volatile uint32_t g_touch_count;

//---------------- globales para tasks ----------------
extern TaskHandle_t g_mathTaskHandle;
extern TaskHandle_t g_speedTaskHandle;

// ---------------- ISR / HALL ----------------
extern portMUX_TYPE g_isr_mux;

extern volatile uint32_t g_last_cycles;
extern volatile uint32_t g_period_cycles;
extern volatile uint32_t g_giro;

extern uint32_t g_cpu_mhz;
extern uint32_t g_min_cycles;

// ---------------- UI STATE ----------------
extern ui_state_t g_ui;
extern SemaphoreHandle_t g_ui_mutex;

// ---------------- sesión/reset ----------------
extern volatile bool sessionActive;
extern volatile bool g_reset_req;
extern volatile bool g_speed_reset_req;

// ---------------- Entrenamiento (usado por events) ----------------
extern float espacio;
extern size_t tiempoHour, tiempoMin, tiempoSec;
extern size_t numSeriesMath, numPasadasMath;
extern bool habChart;

// ---------------- Config/WiFi UI ----------------
extern volatile bool habConfig;
extern volatile bool wifi_ok;

extern char cfg_info[64];
extern char cfg_ip[24];
extern char cfg_ssid[33];

extern char cfg_mac[32];
extern char cfg_fw[32];

extern bool habLocalTime;

// ---------------- WiFi scan list (para dropdown) ----------------
#define MAX_NETS 25
extern volatile bool cfg_scan_ready;     // la UI consume este flag
extern int scan_n;
extern char scan_ssid[MAX_NETS][33];
extern char cfg_dd_opts[1024];           // opciones dropdown "a\nb\nc"
extern volatile bool cfg_dd_opts_ready;  // wifi_mgr: indica que cfg_dd_opts está listo

// ---------------- WiFi actions/payload ----------------
extern volatile bool cfg_entered;
extern volatile bool cfg_need_scan;
extern volatile bool cfg_do_connect;
extern volatile bool cfg_do_disconnect;

extern char pending_ssid[33];
extern char pending_pass[65];

// ---------------- máquina de estados wifi ----------------
typedef enum {
  CFG_WIFI_IDLE = 0,
  CFG_WIFI_SCANNING,
  CFG_WIFI_SCAN_DONE,
  CFG_WIFI_CONNECTING,
  CFG_WIFI_CONNECTED,
  CFG_WIFI_FAIL
} cfg_wifi_state_t;

extern volatile cfg_wifi_state_t cfg_wifi_state;

//---------------- INTERVALADO----------------
typedef enum { TRAIN_NONE=0, TRAIN_FREE, TRAIN_INTERVAL } train_mode_t;
extern volatile train_mode_t g_train_mode;
extern volatile bool g_interval_is_dist; // true si pasada por distancia

//congelado de datos
typedef struct {
  uint32_t total_time_ms;
  float    total_dist_m;
  float    total_kcal;
  float    avg_speed_kmh;
  float    avg_pace_min_km;
} workout_final_t;

extern volatile bool g_workout_frozen;
extern workout_final_t g_workout_final;

extern volatile float g_kcal_total_live;

extern float peso;

//--------rgb gpio-----------
extern volatile uint8_t g_rgb_target_r;
extern volatile uint8_t g_rgb_target_g;
extern volatile uint8_t g_rgb_target_b;

// ---------------- SETPOINTS PROGRAMADO ----------------
#define SET_POINT_SERIES_MAX 5
#define SET_POINT_PASADAS_MAX 20
#define SET_POINT_MACROPAUSE_MAX 5
#define SET_POINT_MICROPAUSE_MAX 5
#define SET_POINT_TIME_PASADA_MAX 10
#define SET_POINT_DISTANCE_PASADA_MAX 1000

extern volatile uint8_t  setPointSeries;
extern volatile uint8_t  setPointPasadas;
extern volatile uint8_t  setPointMacroPausaMin;
extern volatile uint8_t  setPointMicroPausaMin;
extern volatile uint8_t  setPointMicroPausaSeg;
extern volatile uint8_t  setPointTimePasadaMin;
extern volatile uint8_t  setPointTimePasadaSeg;

// ✅ IMPORTANTE: distancia puede ser hasta 1000 -> uint16_t
extern volatile uint16_t setPointDistancePasada;


