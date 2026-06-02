#include "rc_state.h"

#include "main.h"

#define CRSF_CHANNEL_VALUE_MIN 192U
#define CRSF_CHANNEL_VALUE_MID 992U

#define RC_CHANNEL_INDEX_LENKUNG 0U
#define RC_CHANNEL_INDEX_GAS     1U
#define RC_CHANNEL_INDEX_BREMSE  2U
#define RC_CHANNEL_INDEX_CONTROL 3U
#define RC_CONTROL_WORD_MASK     0x07FFU
#define RC_CAMERA_CODE_SHIFT     8U
#define RC_CAMERA_CENTER_CODE    4U
#define RC_CAMERA_MIN_DEG        (-90)
#define RC_CAMERA_STEP_DEG       30
#define RC_BUTTON_FRONT_DIFF     6U
#define RC_BUTTON_REAR_DIFF      7U
#define RC_CONTROL_SEGMENT_COUNT 4U
#define RC_CONTROL_SYMBOL_COUNT  32U
#define RC_CONTROL_SYMBOL_MIN    220U
#define RC_CONTROL_SYMBOL_STEP   48U
#define RC_CONTROL_SYMBOL_TOLERANCE (RC_CONTROL_SYMBOL_STEP / 2U)

static rc_state_t s_rc_state;
static uint16_t s_segmented_control_word = (uint16_t)(RC_CAMERA_CENTER_CODE << RC_CAMERA_CODE_SHIFT);

static uint32_t enter_critical_section(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  return primask;
}

static void exit_critical_section(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

static int clamp_us(int pulse_us)
{
  if (pulse_us < 1000)
  {
    return 1000;
  }

  if (pulse_us > 2000)
  {
    return 2000;
  }

  return pulse_us;
}

static int crsf_to_us(uint16_t value)
{
  return clamp_us((((int)value - (int)CRSF_CHANNEL_VALUE_MID) * 5 / 8) + 1500);
}

static int invert_servo_us(int pulse_us)
{
  return clamp_us(3000 - pulse_us);
}

static uint16_t crsf_to_control_word(uint16_t value)
{
  return value & RC_CONTROL_WORD_MASK;
}

static bool decode_control_symbol(uint16_t value, uint8_t* out_segment, uint8_t* out_payload)
{
  const int raw_symbol =
      ((int)value - (int)RC_CONTROL_SYMBOL_MIN + ((int)RC_CONTROL_SYMBOL_STEP / 2)) /
      (int)RC_CONTROL_SYMBOL_STEP;
  const int expected_value = (int)RC_CONTROL_SYMBOL_MIN + (raw_symbol * (int)RC_CONTROL_SYMBOL_STEP);
  const int error = ((int)value > expected_value) ? ((int)value - expected_value) : (expected_value - (int)value);

  if ((out_segment == 0) || (out_payload == 0))
  {
    return false;
  }

  if ((raw_symbol < 0) || (raw_symbol >= (int)RC_CONTROL_SYMBOL_COUNT))
  {
    return false;
  }

  if (error > (int)RC_CONTROL_SYMBOL_TOLERANCE)
  {
    return false;
  }

  *out_segment = (uint8_t)(((uint32_t)raw_symbol >> 3U) & 0x03U);
  *out_payload = (uint8_t)((uint32_t)raw_symbol & 0x07U);

  return *out_segment < RC_CONTROL_SEGMENT_COUNT;
}

static uint16_t update_segmented_control_word(uint16_t value)
{
  uint8_t segment = 0U;
  uint8_t payload = 0U;
  uint16_t mask = 0U;

  if (!decode_control_symbol(value, &segment, &payload))
  {
    return s_segmented_control_word;
  }

  switch (segment)
  {
    case 0U:
      mask = 0x0007U;
      s_segmented_control_word =
          (uint16_t)((s_segmented_control_word & (uint16_t)~mask) | ((uint16_t)payload & 0x0007U));
      break;
    case 1U:
      mask = 0x0038U;
      s_segmented_control_word =
          (uint16_t)((s_segmented_control_word & (uint16_t)~mask) | (((uint16_t)payload & 0x0007U) << 3U));
      break;
    case 2U:
      mask = 0x00C0U;
      s_segmented_control_word =
          (uint16_t)((s_segmented_control_word & (uint16_t)~mask) | (((uint16_t)payload & 0x0003U) << 6U));
      break;
    case 3U:
      mask = 0x0700U;
      s_segmented_control_word =
          (uint16_t)((s_segmented_control_word & (uint16_t)~mask) | (((uint16_t)payload & 0x0007U) << RC_CAMERA_CODE_SHIFT));
      break;
    default:
      break;
  }

  s_segmented_control_word = crsf_to_control_word(s_segmented_control_word);

  return s_segmented_control_word;
}

static int camera_angle_from_code(uint16_t code)
{
  if ((code < 1U) || (code > 7U))
  {
    code = RC_CAMERA_CENTER_CODE;
  }

  return RC_CAMERA_MIN_DEG + (((int)code - 1) * RC_CAMERA_STEP_DEG);
}

static void fill_default_state(rc_state_t* state)
{
  uint32_t i = 0U;

  for (i = 0U; i < RC_STATE_NUM_CHANNELS; i++)
  {
    state->channels[i] = CRSF_CHANNEL_VALUE_MIN;
  }

  state->channels[0] = CRSF_CHANNEL_VALUE_MID;
  state->channels[1] = CRSF_CHANNEL_VALUE_MID;
  state->channels[2] = CRSF_CHANNEL_VALUE_MIN;
  state->channels[3] = (uint16_t)(RC_CAMERA_CENTER_CODE << RC_CAMERA_CODE_SHIFT);

  state->lenkung_us = 1500;
  state->gas_us = 1500;
  state->bremse_us = 1000;
  state->camera_pan_angle_deg = 0;
  state->diff_front_locked = false;
  state->diff_rear_locked = false;
  state->knopfmaske = 0U;
  state->last_update_ms = 0U;
  state->signal_valid = false;

  for (i = 0U; i < RC_STATE_NUM_BUTTONS; i++)
  {
    state->knopf[i] = false;
  }
}

void rc_state_init(void)
{
  rc_state_t initial_state;
  uint32_t primask = 0U;

  fill_default_state(&initial_state);
  s_segmented_control_word = (uint16_t)(RC_CAMERA_CENTER_CODE << RC_CAMERA_CODE_SHIFT);

  primask = enter_critical_section();
  s_rc_state = initial_state;
  exit_critical_section(primask);
}

void rc_state_update_from_channels(const uint16_t channels[RC_STATE_NUM_CHANNELS], uint32_t now_ms)
{
  rc_state_t next_state;
  uint32_t primask = 0U;
  uint32_t i = 0U;

  fill_default_state(&next_state);

  for (i = 0U; i < RC_STATE_NUM_CHANNELS; i++)
  {
    next_state.channels[i] = channels[i];
  }

  next_state.lenkung_us = invert_servo_us(crsf_to_us(next_state.channels[RC_CHANNEL_INDEX_LENKUNG]));
  next_state.gas_us = crsf_to_us(next_state.channels[RC_CHANNEL_INDEX_GAS]);
  next_state.bremse_us = crsf_to_us(next_state.channels[RC_CHANNEL_INDEX_BREMSE]);

  const uint16_t control_word = update_segmented_control_word(next_state.channels[RC_CHANNEL_INDEX_CONTROL]);
  const uint16_t camera_code = (control_word >> RC_CAMERA_CODE_SHIFT) & 0x07U;

  for (i = 0U; i < RC_STATE_NUM_BUTTONS; i++)
  {
    const bool gedrueckt = ((control_word >> i) & 0x01U) != 0U;

    next_state.knopf[i] = gedrueckt;

    if (gedrueckt)
    {
      next_state.knopfmaske |= (uint16_t)(1U << i);
    }
  }

  next_state.camera_pan_angle_deg = camera_angle_from_code(camera_code);
  next_state.diff_front_locked = next_state.knopf[RC_BUTTON_FRONT_DIFF];
  next_state.diff_rear_locked = next_state.knopf[RC_BUTTON_REAR_DIFF];
  next_state.last_update_ms = now_ms;
  next_state.signal_valid = true;

  primask = enter_critical_section();
  s_rc_state = next_state;
  exit_critical_section(primask);
}

void rc_state_mark_signal_lost(void)
{
  uint32_t primask = enter_critical_section();

  s_rc_state.signal_valid = false;

  exit_critical_section(primask);
}

void rc_state_snapshot(rc_state_t* out_state)
{
  uint32_t primask = 0U;

  if (out_state == 0)
  {
    return;
  }

  primask = enter_critical_section();
  *out_state = s_rc_state;
  exit_critical_section(primask);
}

bool rc_state_signal_is_recent(const rc_state_t* state, uint32_t now_ms, uint32_t timeout_ms)
{
  if ((state == 0) || !state->signal_valid)
  {
    return false;
  }

  return (now_ms - state->last_update_ms) <= timeout_ms;
}
