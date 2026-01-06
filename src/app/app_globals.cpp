#include "app_globals.h"
#include <freertos/FreeRTOS.h>
#include "dataWilli.h"


// --- Firmware version ---
#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

const char* g_fw_version = FW_VERSION;
const uint32_t g_fw_build = (uint32_t)FW_BUILD;

// --- OTA state ---
volatile bool g_ota_available = false;
//volatile bool g_ota_active = false;
volatile int  g_ota_progress = 0;

char g_ota_latest_ver[16] = {0};
char g_ota_bin_url[256]   = {0};
char g_ota_notes[256]     = {0};
char g_ota_status[64]     = {0};

//service
ServiceMgr g_service;

//------------ usadas en mathTask
size_t tiempoHour=0, tiempoMin=0, tiempoSec=0;
float espacio=0;
size_t pausaMacro=0, pausaMicro=0;
size_t fin = 0;
size_t numSeriesMath, numPasadasMath;
bool habLocalTime = 0;
float calorias;
float horasCinta;
bool habChart = 0;

//------------ usadas en speedTask
float drdisco = tita * radio * 0.001;  //distancia radial del disco

//metricas para medicion y testing
volatile uint32_t g_flush_count = 0;
volatile uint32_t g_flush_worst_us = 0;
volatile uint32_t g_flush_last_us = 0;
volatile int32_t  g_flush_worst_w = 0;
volatile int32_t  g_flush_worst_h = 0;
volatile uint32_t g_touch_last_us  = 0;
volatile uint32_t g_touch_worst_us = 0;
volatile uint32_t g_touch_count    = 0;

//---------------- globales para tasks ----------------
TaskHandle_t g_mathTaskHandle = NULL;
TaskHandle_t g_speedTaskHandle = NULL;

// Mutex ISR
portMUX_TYPE g_isr_mux = portMUX_INITIALIZER_UNLOCKED;

// ISR / medición
volatile uint32_t g_last_cycles = 0;
volatile uint32_t g_period_cycles = 0;
volatile uint32_t g_giro = 0;

uint32_t g_cpu_mhz = 240;
uint32_t g_min_cycles = 0;

// UI snapshot
ui_state_t g_ui = {};
SemaphoreHandle_t g_ui_mutex = nullptr;

// sesión/reset
volatile bool sessionActive = false;
volatile bool g_reset_req = false;
volatile bool g_speed_reset_req = false;

// config/wifi UI
volatile bool habConfig = false;
volatile bool wifi_ok = false;
char cfg_info[64] = "";
char cfg_ip[24] = "0.0.0.0";
char cfg_ssid[33] = "SIN RED";

// scan list
volatile bool cfg_scan_ready = false;
int scan_n = 0;
char scan_ssid[MAX_NETS][33] = {0};
char cfg_dd_opts[1024];           // opciones dropdown en formato "a\nb\nc"
volatile bool cfg_dd_opts_ready = false;  // opcional (si querés usarlo aparte)

// workflow config
volatile bool cfg_entered = false;
volatile bool cfg_need_scan = false;
volatile bool cfg_do_connect = false;
volatile bool cfg_do_disconnect = false;

// connect pending
char pending_ssid[33] = "";
char pending_pass[65] = "";

TaskHandle_t g_wifiScanTaskHandle = nullptr;

char cfg_mac[32] = "";
char cfg_fw[32]  = "";

volatile cfg_wifi_state_t cfg_wifi_state;

//INTERVALADO
volatile train_mode_t g_train_mode = TRAIN_NONE;
volatile bool g_interval_is_dist = false;

//congelado de datos
volatile bool g_workout_frozen = false;
workout_final_t g_workout_final = {0};

volatile float g_kcal_total_live = 0.0f;

float peso = 70;

//--------rgb gpio-----------
volatile uint8_t g_rgb_target_r = 0;
volatile uint8_t g_rgb_target_g = 0;
volatile uint8_t g_rgb_target_b = 0;

// ---------------- SETPOINTS PROGRAMADO ----------------
volatile uint8_t  setPointSeries        = 1;
volatile uint8_t  setPointPasadas       = 6;
volatile uint8_t  setPointMacroPausaMin = 1;
volatile uint8_t  setPointMicroPausaMin = 0;
volatile uint8_t  setPointMicroPausaSeg = 30;
volatile uint8_t  setPointTimePasadaMin = 0;
volatile uint8_t  setPointTimePasadaSeg = 30;

// ✅ distancia hasta 1000
volatile uint16_t setPointDistancePasada = 100;

// ---------------- UI request flags ----------------
volatile uint32_t g_ui_req_flags = 0;
