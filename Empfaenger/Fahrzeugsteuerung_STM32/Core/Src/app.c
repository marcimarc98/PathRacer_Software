#include "app.h"
#include "battery_monitor.h"
#include "crsf_telemetry.h"
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
  crsf_vehicle_status_t telemetry_status;
  battery_status_t battery_status;

  rc_state_snapshot(&rc_state);
  battery_monitor_sample();
  battery_monitor_get_status(&battery_status);

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

  telemetry_status.gear = vehicle_command.gear;
  telemetry_status.drive_mode = vehicle_command.drive_mode;
  telemetry_status.neutral_locked = vehicle_command.neutral_locked;
  telemetry_status.camera_rear_active = vehicle_command.camera_rear_active;
  telemetry_status.main_light_on = lights_state.main_light_on;
  telemetry_status.battery_mv = battery_status.battery_mv;
  telemetry_status.battery_percent = battery_status.battery_percent;

  crsf_telemetry_process(now_ms, &telemetry_status);
}

void app_init(void)
{
  rc_state_init();
  battery_monitor_init();
  vehicle_control_init();
  lights_control_init();
  drive_pwm_init();
  crsf_receiver_init();
  crsf_telemetry_init();
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
