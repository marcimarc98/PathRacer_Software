#ifndef CRSF_TELEMETRY_H
#define CRSF_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

#include "vehicle_control.h"

typedef struct
{
  vehicle_gear_t gear;
  vehicle_drive_mode_t drive_mode;
  bool neutral_locked;
  bool camera_rear_active;
  bool main_light_on;
  uint16_t battery_mv;
  uint8_t battery_percent;
  int16_t battery_temp_c;
} crsf_vehicle_status_t;

void crsf_telemetry_init(void);
void crsf_telemetry_process(uint32_t now_ms, const crsf_vehicle_status_t* status);

#endif
