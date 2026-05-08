#ifndef DRIVE_PWM_H
#define DRIVE_PWM_H

#include <stdbool.h>

void drive_pwm_init(void);
void drive_pwm_apply(int servo_us, int esc_us, bool camera_rear_active);
void drive_pwm_apply_failsafe(void);

#endif
