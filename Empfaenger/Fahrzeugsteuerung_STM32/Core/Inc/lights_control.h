#ifndef LIGHTS_CONTROL_H
#define LIGHTS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "rc_state.h"

typedef struct
{
  bool main_light_on;
  bool flash_active;
  bool brake_light_on;
} lights_control_output_t;

void lights_control_init(void);
void lights_control_on_signal_lost(void);
void lights_control_step(const rc_state_t* rc_state, uint32_t now_ms, lights_control_output_t* out_state);

#endif
