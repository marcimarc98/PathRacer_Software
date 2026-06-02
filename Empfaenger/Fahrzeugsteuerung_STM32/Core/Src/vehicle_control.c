#include "vehicle_control.h"
#include "main.h"

#define BUTTON_DESIRED_REVERSE   0U
#define BUTTON_DESIRED_DRIVE     1U
#define BUTTON_CAMERA_REAR       2U
#define BUTTON_NORMAL_MODE       3U

#define PEDAL_MIN_US                   1000
#define PEDAL_MAX_US                   2000
#define CONTROL_NEUTRAL_US             1500
#define GAS_ACTIVE_DEADBAND_US         8
#define BRAKE_ACTIVE_ENTER_DEADBAND_US 80
#define BRAKE_ACTIVE_EXIT_DEADBAND_US  40
#define REVERSE_GAS_LIMIT_PERCENT      20U
#define NORMAL_DRIVE_PROGRESSIVITY_PERCENT 95U
#define NORMAL_DRIVE_LAUNCH_OFFSET_US  50U
#define STEERING_EXPO_PERCENT          65U
#define REVERSE_PRIME_AMOUNT_US        400U
#define REVERSE_PRIME_PULSE_MS         300U
#define REVERSE_PRIME_NEUTRAL_MS       300U

typedef struct
{
  vehicle_gear_t gear;
  vehicle_drive_mode_t drive_mode;
  bool neutral_locked;
  bool camera_rear_active;
  bool drive_brake_cycle_active;
  bool drive_brake_lockout;
  bool brake_input_active;
  bool reverse_prime_active;
  uint32_t reverse_prime_started_ms;
} vehicle_control_state_t;

static vehicle_control_state_t s_vehicle_control;

typedef enum
{
  TRANSMISSION_STATE_NEUTRAL_LOCKED = 0,
  TRANSMISSION_STATE_NEUTRAL_READY,
  TRANSMISSION_STATE_DRIVE,
  TRANSMISSION_STATE_REVERSE
} transmission_state_t;

static transmission_state_t s_transmission_state = TRANSMISSION_STATE_NEUTRAL_LOCKED;

static uint16_t get_pedal_full_scale(void)
{
  return (uint16_t)(PEDAL_MAX_US - PEDAL_MIN_US);
}

static int clamp_us(int value)
{
  if (value < PEDAL_MIN_US)
  {
    return PEDAL_MIN_US;
  }

  if (value > PEDAL_MAX_US)
  {
    return PEDAL_MAX_US;
  }

  return value;
}

static uint16_t pedal_to_amount_with_deadband(int pedal_us, int deadband_us)
{
  int amount = clamp_us(pedal_us) - PEDAL_MIN_US;

  if (amount <= deadband_us)
  {
    return 0U;
  }

  return (uint16_t)amount;
}

static uint16_t pedal_to_amount_raw(int pedal_us)
{
  int amount = clamp_us(pedal_us) - PEDAL_MIN_US;

  if (amount <= 0)
  {
    return 0U;
  }

  return (uint16_t)amount;
}

static bool rc_button_is_pressed(const rc_state_t* rc_state, uint32_t index)
{
  if ((rc_state == 0) || (index >= RC_STATE_NUM_BUTTONS))
  {
    return false;
  }

  return rc_state->knopf[index];
}

static int mirror_around_neutral(int value_us)
{
  return clamp_us((CONTROL_NEUTRAL_US * 2) - value_us);
}

static uint16_t limit_reverse_amount(uint16_t gas_amount)
{
  const uint16_t reverse_limit =
      (uint16_t)(((uint32_t)get_pedal_full_scale() * REVERSE_GAS_LIMIT_PERCENT) / 100U);

  return (gas_amount > reverse_limit) ? reverse_limit : gas_amount;
}

static uint16_t apply_normal_drive_curve(uint16_t gas_amount)
{
  const uint32_t full_scale = (uint32_t)get_pedal_full_scale();
  const uint32_t progressivity = NORMAL_DRIVE_PROGRESSIVITY_PERCENT;
  const uint32_t launch_offset = NORMAL_DRIVE_LAUNCH_OFFSET_US;
  const uint32_t linear_amount = (uint32_t)gas_amount;
  const uint32_t cubic_amount =
      ((uint32_t)gas_amount * (uint32_t)gas_amount * (uint32_t)gas_amount) / (full_scale * full_scale);
  uint32_t curved_amount =
      ((linear_amount * (100U - progressivity)) + (cubic_amount * progressivity)) / 100U;
  uint32_t shifted_amount = 0U;

  if (gas_amount == 0U)
  {
    return 0U;
  }

  if (curved_amount > full_scale)
  {
    curved_amount = full_scale;
  }

  shifted_amount = launch_offset + ((curved_amount * (full_scale - launch_offset)) / full_scale);

  if (shifted_amount > full_scale)
  {
    shifted_amount = full_scale;
  }

  return (uint16_t)shifted_amount;
}

static int apply_steering_expo(int steering_us)
{
  const int clamped_steering_us = clamp_us(steering_us);
  const int centered = clamped_steering_us - CONTROL_NEUTRAL_US;
  const int sign = (centered < 0) ? -1 : 1;
  const uint32_t magnitude = (uint32_t)((centered < 0) ? -centered : centered);
  const uint32_t full_scale = (uint32_t)(PEDAL_MAX_US - CONTROL_NEUTRAL_US);
  const uint32_t expo = STEERING_EXPO_PERCENT;
  const uint64_t linear_part = (uint64_t)magnitude * (uint64_t)(100U - expo);
  const uint64_t cubic_part =
      ((uint64_t)magnitude * (uint64_t)magnitude * (uint64_t)magnitude * (uint64_t)expo) /
      ((uint64_t)full_scale * (uint64_t)full_scale);
  uint64_t expo_magnitude = (linear_part + cubic_part) / 100U;
  int steering_output = 0;

  if (expo_magnitude > full_scale)
  {
    expo_magnitude = (uint64_t)full_scale;
  }

  steering_output = CONTROL_NEUTRAL_US + (sign * (int)expo_magnitude);
  return clamp_us(steering_output);
}

static int apply_steering_curve(int steering_us)
{
  int steering_output =
      (s_vehicle_control.drive_mode == VEHICLE_DRIVE_MODE_SPORT) ? clamp_us(steering_us) : apply_steering_expo(steering_us);

  if (s_vehicle_control.camera_rear_active)
  {
    steering_output = mirror_around_neutral(steering_output);
  }

  return steering_output;
}

static void reset_brake_interlocks(void)
{
  s_vehicle_control.drive_brake_cycle_active = false;
  s_vehicle_control.drive_brake_lockout = true;
  s_vehicle_control.brake_input_active = false;
}

static void enter_locked_neutral(void)
{
  s_transmission_state = TRANSMISSION_STATE_NEUTRAL_LOCKED;
  s_vehicle_control.gear = VEHICLE_GEAR_NEUTRAL;
  s_vehicle_control.neutral_locked = true;
  reset_brake_interlocks();
}

static int mix_drive_esc_output(int gas_us, int brake_us)
{
  uint16_t gas_amount = pedal_to_amount_with_deadband(gas_us, GAS_ACTIVE_DEADBAND_US);
  const uint16_t brake_raw_amount = pedal_to_amount_raw(brake_us);
  uint16_t brake_amount = 0U;

  if (s_vehicle_control.brake_input_active)
  {
    if (brake_raw_amount > BRAKE_ACTIVE_EXIT_DEADBAND_US)
    {
      brake_amount = brake_raw_amount;
    }
    else
    {
      s_vehicle_control.brake_input_active = false;
    }
  }
  else if (brake_raw_amount > BRAKE_ACTIVE_ENTER_DEADBAND_US)
  {
    s_vehicle_control.brake_input_active = true;
    brake_amount = brake_raw_amount;
  }

  if (brake_amount > 0U)
  {
    if ((!s_vehicle_control.drive_brake_lockout) || s_vehicle_control.drive_brake_cycle_active)
    {
      s_vehicle_control.drive_brake_cycle_active = true;
      return CONTROL_NEUTRAL_US - (int)brake_amount;
    }

    return CONTROL_NEUTRAL_US;
  }

  if (s_vehicle_control.drive_brake_cycle_active)
  {
    s_vehicle_control.drive_brake_cycle_active = false;
    s_vehicle_control.drive_brake_lockout = true;
  }

  if (gas_amount > 0U)
  {
    s_vehicle_control.drive_brake_lockout = false;

    if (s_vehicle_control.drive_mode == VEHICLE_DRIVE_MODE_NORMAL)
    {
      gas_amount = apply_normal_drive_curve(gas_amount);
    }

    return CONTROL_NEUTRAL_US + (int)gas_amount;
  }

  return CONTROL_NEUTRAL_US;
}

static int mix_reverse_esc_output(int gas_us, int brake_us)
{
  uint16_t limited_gas_amount = limit_reverse_amount(pedal_to_amount_with_deadband(gas_us, GAS_ACTIVE_DEADBAND_US));

  (void)brake_us;

  if (limited_gas_amount > 0U)
  {
    return CONTROL_NEUTRAL_US - (int)limited_gas_amount;
  }

  return CONTROL_NEUTRAL_US;
}

static void start_reverse_prime(void)
{
  s_vehicle_control.reverse_prime_active = true;
  s_vehicle_control.reverse_prime_started_ms = HAL_GetTick();
}

static void stop_reverse_prime(void)
{
  s_vehicle_control.reverse_prime_active = false;
  s_vehicle_control.reverse_prime_started_ms = 0U;
}

static int mix_reverse_esc_output_with_prime(int gas_us, int brake_us)
{
  if (s_vehicle_control.reverse_prime_active)
  {
    const uint32_t elapsed_ms = HAL_GetTick() - s_vehicle_control.reverse_prime_started_ms;

    if (elapsed_ms < REVERSE_PRIME_PULSE_MS)
    {
      (void)gas_us;
      (void)brake_us;
      return CONTROL_NEUTRAL_US - (int)REVERSE_PRIME_AMOUNT_US;
    }

    if (elapsed_ms < (REVERSE_PRIME_PULSE_MS + REVERSE_PRIME_NEUTRAL_MS))
    {
      (void)gas_us;
      (void)brake_us;
      return CONTROL_NEUTRAL_US;
    }

    stop_reverse_prime();
  }

  return mix_reverse_esc_output(gas_us, brake_us);
}

static int mix_esc_output(vehicle_gear_t gear, int gas_us, int brake_us)
{
  if (gear == VEHICLE_GEAR_DRIVE)
  {
    return mix_drive_esc_output(gas_us, brake_us);
  }

  if (gear == VEHICLE_GEAR_REVERSE)
  {
    return mix_reverse_esc_output_with_prime(gas_us, brake_us);
  }

  stop_reverse_prime();
  reset_brake_interlocks();
  return CONTROL_NEUTRAL_US;
}

static void update_transmission_state(
    vehicle_gear_t desired_gear,
    bool neutral_unlocked)
{
  const vehicle_gear_t previous_gear = s_vehicle_control.gear;

  if (!neutral_unlocked)
  {
    enter_locked_neutral();
    return;
  }

  switch (desired_gear)
  {
    case VEHICLE_GEAR_DRIVE:
      s_transmission_state = TRANSMISSION_STATE_DRIVE;
      s_vehicle_control.gear = VEHICLE_GEAR_DRIVE;
      s_vehicle_control.neutral_locked = false;
      stop_reverse_prime();
      break;

    case VEHICLE_GEAR_REVERSE:
      s_transmission_state = TRANSMISSION_STATE_REVERSE;
      s_vehicle_control.gear = VEHICLE_GEAR_REVERSE;
      s_vehicle_control.neutral_locked = false;
      if (previous_gear == VEHICLE_GEAR_NEUTRAL)
      {
        start_reverse_prime();
      }
      else
      {
        stop_reverse_prime();
      }
      break;

    case VEHICLE_GEAR_NEUTRAL:
    default:
      s_transmission_state = TRANSMISSION_STATE_NEUTRAL_READY;
      s_vehicle_control.gear = VEHICLE_GEAR_NEUTRAL;
      s_vehicle_control.neutral_locked = false;
      break;
  }

  if (previous_gear != s_vehicle_control.gear)
  {
    reset_brake_interlocks();
  }
}

static vehicle_gear_t get_desired_gear(const rc_state_t* rc_state)
{
  const bool reverse_selected = rc_button_is_pressed(rc_state, BUTTON_DESIRED_REVERSE);
  const bool drive_selected = rc_button_is_pressed(rc_state, BUTTON_DESIRED_DRIVE);

  if (reverse_selected == drive_selected)
  {
    return VEHICLE_GEAR_NEUTRAL;
  }

  return reverse_selected ? VEHICLE_GEAR_REVERSE : VEHICLE_GEAR_DRIVE;
}

static vehicle_drive_mode_t get_desired_drive_mode(const rc_state_t* rc_state)
{
  return rc_button_is_pressed(rc_state, BUTTON_NORMAL_MODE) ? VEHICLE_DRIVE_MODE_NORMAL : VEHICLE_DRIVE_MODE_SPORT;
}

static bool get_desired_camera_rear_active(const rc_state_t* rc_state)
{
  return rc_button_is_pressed(rc_state, BUTTON_CAMERA_REAR);
}

void vehicle_control_init(void)
{
  enter_locked_neutral();
  s_vehicle_control.drive_mode = VEHICLE_DRIVE_MODE_NORMAL;
  s_vehicle_control.camera_rear_active = false;
  stop_reverse_prime();
  reset_brake_interlocks();
}

void vehicle_control_on_signal_lost(void)
{
  stop_reverse_prime();
}

void vehicle_control_get_status(vehicle_command_t* out_command)
{
  if (out_command == 0)
  {
    return;
  }

  out_command->lenkung_us = CONTROL_NEUTRAL_US;
  out_command->esc_us = CONTROL_NEUTRAL_US;
  out_command->gear = s_vehicle_control.gear;
  out_command->drive_mode = s_vehicle_control.drive_mode;
  out_command->neutral_locked = s_vehicle_control.neutral_locked;
  out_command->camera_rear_active = s_vehicle_control.camera_rear_active;
  out_command->camera_pan_angle_deg = 0;
  out_command->diff_front_locked = false;
  out_command->diff_rear_locked = false;
}

void vehicle_control_step(const rc_state_t* rc_state, vehicle_command_t* out_command)
{
  const vehicle_gear_t desired_gear = get_desired_gear(rc_state);
  const bool desired_neutral_unlocked = desired_gear != VEHICLE_GEAR_NEUTRAL;
  const vehicle_drive_mode_t desired_drive_mode = get_desired_drive_mode(rc_state);
  const bool desired_camera_rear_active = get_desired_camera_rear_active(rc_state);

  if (out_command == 0)
  {
    return;
  }

  s_vehicle_control.drive_mode = desired_drive_mode;
  s_vehicle_control.camera_rear_active = desired_camera_rear_active;
  update_transmission_state(desired_gear, desired_neutral_unlocked);

  out_command->lenkung_us = (rc_state != 0) ? apply_steering_curve(rc_state->lenkung_us) : CONTROL_NEUTRAL_US;
  out_command->esc_us =
      (rc_state != 0) ? mix_esc_output(s_vehicle_control.gear, rc_state->gas_us, rc_state->bremse_us) : CONTROL_NEUTRAL_US;
  out_command->gear = s_vehicle_control.gear;
  out_command->drive_mode = s_vehicle_control.drive_mode;
  out_command->neutral_locked = s_vehicle_control.neutral_locked;
  out_command->camera_rear_active = s_vehicle_control.camera_rear_active;
  out_command->camera_pan_angle_deg = (rc_state != 0) ? rc_state->camera_pan_angle_deg : 0;
  out_command->diff_front_locked = (rc_state != 0) && rc_state->diff_front_locked;
  out_command->diff_rear_locked = (rc_state != 0) && rc_state->diff_rear_locked;
}
