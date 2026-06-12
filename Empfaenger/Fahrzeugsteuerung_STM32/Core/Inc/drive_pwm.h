#ifndef DRIVE_PWM_H
#define DRIVE_PWM_H

#include <stdbool.h>

/* Initialisiert Timer und GPIOs fuer Servo-, ESC-, Kamera- und Diff-PWM. */
void drive_pwm_init(void);

/* Schreibt die berechneten Fahrzeugbefehle auf alle PWM-Ausgaenge. */
void drive_pwm_apply(
    int servo_us,
    int esc_us,
    bool camera_rear_active,
    int camera_pan_angle_deg,
    bool diff_front_locked,
    bool diff_rear_locked);

/* Setzt Antrieb und Lenkung neutral, laesst Zusatzstellungen aber erhalten. */
void drive_pwm_apply_failsafe(void);

#endif
