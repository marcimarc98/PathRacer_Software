#include "lights_control.h"

#include "main.h"

#define BUTTON_FLASH_ACTIVE            4U
#define BUTTON_MAIN_LIGHT_ON           5U
#define BRAKE_LIGHT_ACTIVE_DEADBAND_US 25
#define BRAKE_FULL_ACTIVE_THRESHOLD_US 1950
#define BRAKE_LIGHT_MIN_ON_MS          1000U
#define ADAPTIVE_BRAKE_BLINK_MS        150U
#define REAR_LIGHT_PWM_PERIOD_MS       10U
#define REAR_LIGHT_PWM_OFF_DUTY_MS     3U

static bool rc_button_is_pressed(const rc_state_t* rc_state, uint32_t index)
{
  if ((rc_state == 0) || (index >= RC_STATE_NUM_BUTTONS))
  {
    return false;
  }

  return rc_state->knopf[index];
}

static bool brake_is_active(const rc_state_t* rc_state)
{
  if (rc_state == 0)
  {
    return false;
  }

  return rc_state->bremse_us > (1000 + BRAKE_LIGHT_ACTIVE_DEADBAND_US);
}

static bool brake_is_full_active(const rc_state_t* rc_state)
{
  if (rc_state == 0)
  {
    return false;
  }

  return rc_state->bremse_us >= BRAKE_FULL_ACTIVE_THRESHOLD_US;
}

static bool rear_light_pwm_output_for_now(
    uint32_t now_ms,
    bool tail_light_on,
    bool brake_light_active,
    bool adaptive_blinking)
{
  if (adaptive_blinking)
  {
    const uint32_t blink_phase = (now_ms / ADAPTIVE_BRAKE_BLINK_MS) % 2U;
    const bool blink_on_phase = (blink_phase == 0U);

    if (blink_on_phase)
    {
      return true;
    }

    if (!tail_light_on)
    {
      return false;
    }

    return (now_ms % REAR_LIGHT_PWM_PERIOD_MS) < REAR_LIGHT_PWM_OFF_DUTY_MS;
  }

  if (brake_light_active)
  {
    return true;
  }

  if (!tail_light_on)
  {
    return false;
  }

  return (now_ms % REAR_LIGHT_PWM_PERIOD_MS) < REAR_LIGHT_PWM_OFF_DUTY_MS;
}

static void set_light_outputs(bool front_light_on, bool rear_light_output_on)
{
  HAL_GPIO_WritePin(LIGHT_MAIN_GPIO_Port, LIGHT_MAIN_Pin, front_light_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LIGHT_BRAKE_GPIO_Port, LIGHT_BRAKE_Pin, rear_light_output_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool s_rear_light_hold_active = false;
static uint32_t s_rear_light_last_brake_ms = 0U;

void lights_control_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = LIGHT_MAIN_Pin | LIGHT_BRAKE_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);

  set_light_outputs(false, false);
}

void lights_control_on_signal_lost(void)
{
  s_rear_light_hold_active = false;
  s_rear_light_last_brake_ms = 0U;
  set_light_outputs(false, false);
}

void lights_control_step(const rc_state_t* rc_state, uint32_t now_ms, lights_control_output_t* out_state)
{
  const bool flash_active = rc_button_is_pressed(rc_state, BUTTON_FLASH_ACTIVE);
  const bool main_light_on = rc_button_is_pressed(rc_state, BUTTON_MAIN_LIGHT_ON);
  const bool brake_pressed = brake_is_active(rc_state);
  const bool full_brake_pressed = brake_is_full_active(rc_state);
  const bool front_light_on = main_light_on || flash_active;
  bool brake_light_active = false;
  bool rear_light_output_on = false;
  bool adaptive_blinking = false;

  if (out_state == 0)
  {
    return;
  }

  if (brake_pressed)
  {
    s_rear_light_hold_active = true;
    s_rear_light_last_brake_ms = now_ms;
  }
  else if (s_rear_light_hold_active && ((now_ms - s_rear_light_last_brake_ms) >= BRAKE_LIGHT_MIN_ON_MS))
  {
    s_rear_light_hold_active = false;
  }

  brake_light_active = s_rear_light_hold_active;
  adaptive_blinking = full_brake_pressed;
  rear_light_output_on = rear_light_pwm_output_for_now(now_ms, main_light_on, brake_light_active, adaptive_blinking);

  set_light_outputs(front_light_on, rear_light_output_on);

  out_state->main_light_on = main_light_on;
  out_state->flash_active = flash_active;
  out_state->brake_light_on = brake_light_active;
}
