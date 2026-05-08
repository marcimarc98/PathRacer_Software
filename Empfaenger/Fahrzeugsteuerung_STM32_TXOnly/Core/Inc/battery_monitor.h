#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>

typedef struct
{
  uint16_t battery_mv;
  uint8_t battery_percent;
} battery_status_t;

void battery_monitor_init(void);
void battery_monitor_sample(void);
void battery_monitor_get_status(battery_status_t* out_status);

#endif
