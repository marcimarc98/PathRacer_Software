#include "rc_state.h"

#include "main.h"

#define CRSF_CHANNEL_VALUE_MIN 192U
#define CRSF_CHANNEL_VALUE_MID 992U
#define CRSF_CHANNEL_VALUE_MAX 1792U

#define RC_CHANNEL_INDEX_LENKUNG 0U
#define RC_CHANNEL_INDEX_GAS     1U
#define RC_CHANNEL_INDEX_BREMSE  2U

static const uint8_t s_button_channel_map[RC_STATE_NUM_BUTTONS] = {
    3U,  /* Button 0 -> CH4  (Down Shift) */
    4U,  /* Button 1 -> CH5  (Up Shift) */
    5U,  /* Button 2 -> CH6  (R2) */
    6U,  /* Button 3 -> CH7  (L2) */
    7U,  /* Button 4 -> CH8  (L1) */
    8U,  /* Button 5 -> CH9  (R1) */
    9U   /* Button 6 -> CH10 (PS) */
};

static rc_state_t s_rc_state;

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

static bool crsf_to_button(uint16_t value)
{
  return value >= ((CRSF_CHANNEL_VALUE_MIN + CRSF_CHANNEL_VALUE_MAX) / 2U);
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

  state->lenkung_us = 1500;
  state->gas_us = 1500;
  state->bremse_us = 1000;
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

  for (i = 0U; i < RC_STATE_NUM_BUTTONS; i++)
  {
    const uint8_t channel_index = s_button_channel_map[i];
    const bool gedrueckt =
        (channel_index < RC_STATE_NUM_CHANNELS) ? crsf_to_button(next_state.channels[channel_index]) : false;

    next_state.knopf[i] = gedrueckt;

    if (gedrueckt)
    {
      next_state.knopfmaske |= (uint16_t)(1U << i);
    }
  }

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
