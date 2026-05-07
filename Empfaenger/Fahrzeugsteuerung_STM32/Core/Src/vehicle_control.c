#include "vehicle_control.h"

#define BUTTON_DOWN_SHIFT 0U
#define BUTTON_UP_SHIFT   1U
#define BUTTON_R2         2U
#define BUTTON_L2         3U
#define BUTTON_PS         6U

#define PEDAL_MIN_US                   1000
#define PEDAL_MAX_US                   2000
#define CONTROL_NEUTRAL_US             1500
#define GAS_ACTIVE_DEADBAND_US         8
#define BRAKE_ACTIVE_DEADBAND_US       25
#define REVERSE_GAS_LIMIT_PERCENT      20U
#define NORMAL_DRIVE_PROGRESSIVITY_PERCENT 95U
#define STEERING_EXPO_PERCENT          65U

typedef struct
{
  vehicle_gear_t gear;
  vehicle_drive_mode_t drive_mode;
  bool neutral_locked;
  bool camera_rear_active;
  bool sync_buttons_on_next_step;
  bool prev_down_shift;
  bool prev_up_shift;
  bool prev_mode_toggle;
  bool prev_camera_toggle;
  bool prev_safe_neutral;
  bool drive_brake_cycle_active;
  bool drive_brake_lockout;
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

static bool rc_button_is_pressed(const rc_state_t* rc_state, uint32_t index)
{
  if ((rc_state == 0) || (index >= RC_STATE_NUM_BUTTONS))
  {
    return false;
  }

  return rc_state->knopf[index];
}

static bool button_rising_edge(bool current_value, bool* previous_value)
{
  const bool rising_edge = current_value && !(*previous_value);

  *previous_value = current_value;
  return rising_edge;
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
  const uint32_t linear_amount = (uint32_t)gas_amount;
  const uint32_t cubic_amount =
      ((uint32_t)gas_amount * (uint32_t)gas_amount * (uint32_t)gas_amount) / (full_scale * full_scale);
  uint32_t curved_amount =
      ((linear_amount * (100U - progressivity)) + (cubic_amount * progressivity)) / 100U;

  if (curved_amount > full_scale)
  {
    curved_amount = full_scale;
  }

  return (uint16_t)curved_amount;
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
  const uint16_t brake_amount = pedal_to_amount_with_deadband(brake_us, BRAKE_ACTIVE_DEADBAND_US);

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

static int mix_esc_output(vehicle_gear_t gear, int gas_us, int brake_us)
{
  if (gear == VEHICLE_GEAR_DRIVE)
  {
    return mix_drive_esc_output(gas_us, brake_us);
  }

  if (gear == VEHICLE_GEAR_REVERSE)
  {
    return mix_reverse_esc_output(gas_us, brake_us);
  }

  reset_brake_interlocks();
  return CONTROL_NEUTRAL_US;
}

static void update_transmission_state(
    bool down_shift_edge,
    bool up_shift_edge,
    bool safe_neutral_edge)
{
  if (safe_neutral_edge)
  {
    enter_locked_neutral();
    return;
  }

  switch (s_transmission_state)
  {
    case TRANSMISSION_STATE_NEUTRAL_LOCKED:
      break;

    case TRANSMISSION_STATE_NEUTRAL_READY:
      if (down_shift_edge)
      {
        s_transmission_state = TRANSMISSION_STATE_REVERSE;
        s_vehicle_control.gear = VEHICLE_GEAR_REVERSE;
        s_vehicle_control.neutral_locked = false;
      }
      else if (up_shift_edge)
      {
        s_transmission_state = TRANSMISSION_STATE_DRIVE;
        s_vehicle_control.gear = VEHICLE_GEAR_DRIVE;
        s_vehicle_control.neutral_locked = false;
      }
      break;

    case TRANSMISSION_STATE_DRIVE:
      if (down_shift_edge)
      {
        s_transmission_state = TRANSMISSION_STATE_REVERSE;
        s_vehicle_control.gear = VEHICLE_GEAR_REVERSE;
        s_vehicle_control.neutral_locked = false;
      }
      break;

    case TRANSMISSION_STATE_REVERSE:
      if (up_shift_edge)
      {
        s_transmission_state = TRANSMISSION_STATE_DRIVE;
        s_vehicle_control.gear = VEHICLE_GEAR_DRIVE;
        s_vehicle_control.neutral_locked = false;
      }
      break;

    default:
      enter_locked_neutral();
      break;
  }
}

void vehicle_control_init(void)
{
  enter_locked_neutral();
  s_vehicle_control.drive_mode = VEHICLE_DRIVE_MODE_NORMAL;
  s_vehicle_control.camera_rear_active = false;
  s_vehicle_control.sync_buttons_on_next_step = true;
  s_vehicle_control.prev_down_shift = false;
  s_vehicle_control.prev_up_shift = false;
  s_vehicle_control.prev_mode_toggle = false;
  s_vehicle_control.prev_camera_toggle = false;
  s_vehicle_control.prev_safe_neutral = false;
  reset_brake_interlocks();
}

void vehicle_control_on_signal_lost(void)
{
  s_vehicle_control.sync_buttons_on_next_step = true;
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
}

void vehicle_control_step(const rc_state_t* rc_state, bool unlock_combo_edge, vehicle_command_t* out_command)
{
  const bool down_shift = rc_button_is_pressed(rc_state, BUTTON_DOWN_SHIFT);
  const bool up_shift = rc_button_is_pressed(rc_state, BUTTON_UP_SHIFT);
  const bool camera_toggle = rc_button_is_pressed(rc_state, BUTTON_R2);
  const bool mode_toggle = rc_button_is_pressed(rc_state, BUTTON_L2);
  const bool safe_neutral = rc_button_is_pressed(rc_state, BUTTON_PS);
  bool safe_neutral_edge = false;
  bool down_shift_edge = false;
  bool up_shift_edge = false;
  bool mode_toggle_edge = false;
  bool camera_toggle_edge = false;

  if (out_command == 0)
  {
    return;
  }

  if (s_vehicle_control.sync_buttons_on_next_step)
  {
    s_vehicle_control.prev_down_shift = down_shift;
    s_vehicle_control.prev_up_shift = up_shift;
    s_vehicle_control.prev_camera_toggle = camera_toggle;
    s_vehicle_control.prev_mode_toggle = mode_toggle;
    s_vehicle_control.prev_safe_neutral = safe_neutral;
    s_vehicle_control.sync_buttons_on_next_step = false;
  }
  else
  {
    safe_neutral_edge = button_rising_edge(safe_neutral, &s_vehicle_control.prev_safe_neutral);
    down_shift_edge = button_rising_edge(down_shift, &s_vehicle_control.prev_down_shift);
    up_shift_edge = button_rising_edge(up_shift, &s_vehicle_control.prev_up_shift);
    camera_toggle_edge = button_rising_edge(camera_toggle, &s_vehicle_control.prev_camera_toggle);
    mode_toggle_edge = button_rising_edge(mode_toggle, &s_vehicle_control.prev_mode_toggle);
  }

  if (mode_toggle_edge)
  {
    s_vehicle_control.drive_mode =
        (s_vehicle_control.drive_mode == VEHICLE_DRIVE_MODE_SPORT) ? VEHICLE_DRIVE_MODE_NORMAL : VEHICLE_DRIVE_MODE_SPORT;
  }

  if (camera_toggle_edge)
  {
    s_vehicle_control.camera_rear_active = !s_vehicle_control.camera_rear_active;
  }

  update_transmission_state(down_shift_edge, up_shift_edge, safe_neutral_edge);

  if (unlock_combo_edge)
  {
    if (s_transmission_state == TRANSMISSION_STATE_NEUTRAL_LOCKED)
    {
      s_transmission_state = TRANSMISSION_STATE_NEUTRAL_READY;
      s_vehicle_control.gear = VEHICLE_GEAR_NEUTRAL;
      s_vehicle_control.neutral_locked = false;
      reset_brake_interlocks();
    }
    else
    {
      enter_locked_neutral();
    }
  }

  out_command->lenkung_us = (rc_state != 0) ? apply_steering_curve(rc_state->lenkung_us) : CONTROL_NEUTRAL_US;
  out_command->esc_us =
      (rc_state != 0) ? mix_esc_output(s_vehicle_control.gear, rc_state->gas_us, rc_state->bremse_us) : CONTROL_NEUTRAL_US;
  out_command->gear = s_vehicle_control.gear;
  out_command->drive_mode = s_vehicle_control.drive_mode;
  out_command->neutral_locked = s_vehicle_control.neutral_locked;
  out_command->camera_rear_active = s_vehicle_control.camera_rear_active;
}
