#ifndef DRIVE_PWM_H
#define DRIVE_PWM_H

void drive_pwm_init(void);
void drive_pwm_apply(int servo_us, int esc_us);
void drive_pwm_apply_failsafe(void);

#endif
