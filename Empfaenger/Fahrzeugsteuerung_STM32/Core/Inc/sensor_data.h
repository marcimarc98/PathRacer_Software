#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>

typedef struct
{
  uint16_t battery_mv;
  uint8_t battery_percent;
  int16_t battery_temp_c;
} sensor_data_status_t;

void sensor_data_init(void);
void sensor_data_sample(void);
void sensor_data_get_status(sensor_data_status_t* out_status);

#endif
