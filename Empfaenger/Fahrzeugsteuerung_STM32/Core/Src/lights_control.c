#include "lights_control.h"

#include "main.h"

#define BUTTON_FLASH_ACTIVE            4U
#define BUTTON_MAIN_LIGHT_ON           5U
#define BRAKE_LIGHT_ACTIVE_DEADBAND_US 25

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

static void set_light_outputs(bool main_light_effective_on, bool brake_light_on)
{
  HAL_GPIO_WritePin(LIGHT_MAIN_GPIO_Port, LIGHT_MAIN_Pin, main_light_effective_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LIGHT_BRAKE_GPIO_Port, LIGHT_BRAKE_Pin, brake_light_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

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
  set_light_outputs(false, false);
}

void lights_control_step(const rc_state_t* rc_state, uint32_t now_ms, lights_control_output_t* out_state)
{
  const bool flash_active = rc_button_is_pressed(rc_state, BUTTON_FLASH_ACTIVE);
  const bool main_light_on = rc_button_is_pressed(rc_state, BUTTON_MAIN_LIGHT_ON);
  bool brake_light_on = brake_is_active(rc_state);
  bool main_light_effective_on = main_light_on || flash_active;

  (void)now_ms;

  if (out_state == 0)
  {
    return;
  }

  set_light_outputs(main_light_effective_on, brake_light_on);

  out_state->main_light_on = main_light_on;
  out_state->flash_active = flash_active;
  out_state->brake_light_on = brake_light_on;
}
