#ifndef VEHICLE_CONTROL_H
#define VEHICLE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "rc_state.h"

typedef enum
{
  VEHICLE_GEAR_REVERSE = -1,
  VEHICLE_GEAR_NEUTRAL = 0,
  VEHICLE_GEAR_DRIVE = 1
} vehicle_gear_t;

typedef struct
{
  int lenkung_us;
  int esc_us;
  vehicle_gear_t gear;
  bool neutral_locked;
} vehicle_command_t;

void vehicle_control_init(void);
void vehicle_control_on_signal_lost(void);
void vehicle_control_step(const rc_state_t* rc_state, vehicle_command_t* out_command);

#endif
