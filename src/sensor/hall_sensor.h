#pragma once
#include <stdint.h>

void hall_sensor_init(int gpio);
void hall_sensor_enable(bool en, int gpio);
void hall_sensor_reset();
void hall_sensor_snapshot(uint32_t *giro, uint32_t *period_cycles);
