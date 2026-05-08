#include "app.h"
#include "crsf_receiver.h"
#include "drive_pwm.h"
#include "lights_control.h"
#include "main.h"
#include "rc_state.h"
#include "vehicle_control.h"

#define RC_SIGNAL_TIMEOUT_MS 500U

static void app_run_control_cycle(void)
{
  const uint32_t now_ms = HAL_GetTick();
  rc_state_t rc_state;
  vehicle_command_t vehicle_command;
  lights_control_output_t lights_state;

  rc_state_snapshot(&rc_state);

  if (rc_state_signal_is_recent(&rc_state, now_ms, RC_SIGNAL_TIMEOUT_MS))
  {
    lights_control_step(&rc_state, now_ms, &lights_state);
    vehicle_control_step(&rc_state, lights_state.shift_combo_edge, &vehicle_command);
    drive_pwm_apply(vehicle_command.lenkung_us, vehicle_command.esc_us, vehicle_command.camera_rear_active);
  }
  else
  {
    rc_state_mark_signal_lost();
    vehicle_control_on_signal_lost();
    lights_control_on_signal_lost();
    lights_control_step(0, now_ms, &lights_state);
    vehicle_control_get_status(&vehicle_command);
    drive_pwm_apply_failsafe();
  }
}

void app_init(void)
{
  rc_state_init();
  vehicle_control_init();
  lights_control_init();
  drive_pwm_init();
  crsf_receiver_init();
  app_run_control_cycle();
}

void app_loop(void)
{
  app_run_control_cycle();
}

void app_uart_irq_handler(void)
{
  crsf_receiver_irq_handler();
}
