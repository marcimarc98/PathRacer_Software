#ifndef DRIVE_PWM_H
#define DRIVE_PWM_H

#include <stdbool.h>

void drive_pwm_init(void);
void drive_pwm_apply(
    int servo_us,
    int esc_us,
    bool camera_rear_active,
    int camera_pan_angle_deg,
    bool diff_front_locked,
    bool diff_rear_locked);
void drive_pwm_apply_failsafe(void);

#endif
