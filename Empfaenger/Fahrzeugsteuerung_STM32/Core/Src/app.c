#include "app.h"
#include "crsf_telemetry.h"
#include "crsf_receiver.h"
#include "drive_pwm.h"
#include "lights_control.h"
#include "main.h"
#include "rc_state.h"
#include "sensor_data.h"
#include "vehicle_control.h"

#define RC_SIGNAL_TIMEOUT_MS 500U

/* Zentraler Ablauf des Fahrzeugcontrollers:
 * CRSF-Zustand lesen, Sensoren aktualisieren, Ausgaenge setzen und Telemetrie erzeugen.
 */
static void app_run_control_cycle(void)
{
  const uint32_t now_ms = HAL_GetTick();
  rc_state_t rc_state;
  vehicle_command_t vehicle_command;
  lights_control_output_t lights_state;
  crsf_vehicle_status_t telemetry_status;
  sensor_data_status_t sensor_status;

  rc_state_snapshot(&rc_state);
  sensor_data_sample();
  sensor_data_get_status(&sensor_status);

  /* Nur mit aktuellem RC-Signal werden Fahr- und Lichtbefehle umgesetzt.
   * Bei Timeout geht das Fahrzeug in Failsafe.
   */
  if (rc_state_signal_is_recent(&rc_state, now_ms, RC_SIGNAL_TIMEOUT_MS))
  {
    lights_control_step(&rc_state, now_ms, &lights_state);
    vehicle_control_step(&rc_state, &vehicle_command);
    drive_pwm_apply(
        vehicle_command.lenkung_us,
        vehicle_command.esc_us,
        vehicle_command.camera_rear_active,
        vehicle_command.camera_pan_angle_deg,
        vehicle_command.diff_front_locked,
        vehicle_command.diff_rear_locked);
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

  telemetry_status.battery_mv = sensor_status.battery_mv;
  telemetry_status.battery_percent = sensor_status.battery_percent;
  telemetry_status.battery_temp_c = sensor_status.battery_temp_c;

  crsf_telemetry_process(now_ms, &telemetry_status);
}

/* Initialisiert alle Projektmodule in der Reihenfolge Daten, Logik, Ausgaenge, Funk. */
void app_init(void)
{
  rc_state_init();
  sensor_data_init();
  vehicle_control_init();
  lights_control_init();
  drive_pwm_init();
  crsf_receiver_init();
  crsf_telemetry_init();
  app_run_control_cycle();
}

/* Wird aus main() fortlaufend aufgerufen und fuehrt einen kompletten Regelzyklus aus. */
void app_loop(void)
{
  app_run_control_cycle();
}

/* Weiterleitung des USART-Interrupts an den CRSF-Parser. */
void app_uart_irq_handler(void)
{
  crsf_receiver_irq_handler();
}
