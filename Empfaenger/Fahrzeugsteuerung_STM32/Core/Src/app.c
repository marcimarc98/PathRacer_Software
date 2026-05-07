#include "app.h"
#include "crsf_receiver.h"
#include "drive_pwm.h"
#include "main.h"
#include "rc_state.h"
#include "vehicle_control.h"

#define RC_SIGNAL_TIMEOUT_MS 500U

static void app_run_control_cycle(void)
{
  rc_state_t rc_state;
  vehicle_command_t vehicle_command;

  rc_state_snapshot(&rc_state);

  if (rc_state_signal_is_recent(&rc_state, HAL_GetTick(), RC_SIGNAL_TIMEOUT_MS))
  {
    vehicle_control_step(&rc_state, &vehicle_command);
    drive_pwm_apply(vehicle_command.lenkung_us, vehicle_command.esc_us);
    return;
  }

  rc_state_mark_signal_lost();
  vehicle_control_on_signal_lost();
  drive_pwm_apply_failsafe();
}

void app_init(void)
{
  rc_state_init();
  vehicle_control_init();
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
