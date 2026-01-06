#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ui_state.h"

typedef enum {
  TI_PASS_BY_TIME = 0,
  TI_PASS_BY_DIST
} ti_pass_mode_t;

typedef struct {
  uint8_t num_series;
  uint8_t num_passes;

  uint32_t rest_series_ms;
  uint32_t rest_pass_ms;

  ti_pass_mode_t mode;

  uint32_t pass_time_ms;
  float pass_dist_m;        // metros
} ti_cfg_t;

typedef enum {
  TI_IDLE = 0,
  TI_ARMED,
  TI_RUN,
  TI_REST_PASS,
  TI_REST_SERIES,
  TI_DONE
} ti_state_t;

void ti_init(void);
void ti_apply_cfg(const ti_cfg_t *cfg);
void ti_arm(void);
void ti_rearm(float espacio_m_now);
void ti_on_session_started(float espacio_m_now);
void ti_stop(void);
void ti_update(uint32_t dt_ms, float espacio_m_now);
void ti_reset_stats(void);
ti_state_t ti_get_state(void);

// ✅ NUEVO: aplica a 'out' lo que deba mostrar el intervalado (pausa y/o tiempo restante)
void ti_apply_overlay_to_ui(ui_state_t *out);
