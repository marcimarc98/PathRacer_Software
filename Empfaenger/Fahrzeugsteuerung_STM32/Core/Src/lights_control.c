#include "lights_control.h"

#include "main.h"

#define BUTTON_L1 4U
#define BUTTON_R1 5U

#define LIGHTS_COMBO_WINDOW_MS         200U
#define LIGHTS_FLASH_DURATION_MS       180U
#define BRAKE_LIGHT_ACTIVE_DEADBAND_US 25

typedef struct
{
  bool sync_buttons_on_next_step;
  bool prev_l1;
  bool prev_r1;
  bool l1_pending;
  bool r1_pending;
  bool combo_latched;
  uint32_t l1_pressed_ms;
  uint32_t r1_pressed_ms;
  bool main_light_on;
  bool flash_active;
  uint32_t flash_until_ms;
} lights_control_state_t;

static lights_control_state_t s_lights;

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

  s_lights.sync_buttons_on_next_step = true;
  s_lights.prev_l1 = false;
  s_lights.prev_r1 = false;
  s_lights.l1_pending = false;
  s_lights.r1_pending = false;
  s_lights.combo_latched = false;
  s_lights.l1_pressed_ms = 0U;
  s_lights.r1_pressed_ms = 0U;
  s_lights.main_light_on = false;
  s_lights.flash_active = false;
  s_lights.flash_until_ms = 0U;

  set_light_outputs(false, false);
}

void lights_control_on_signal_lost(void)
{
  s_lights.sync_buttons_on_next_step = true;
  s_lights.l1_pending = false;
  s_lights.r1_pending = false;
  s_lights.combo_latched = false;
}

void lights_control_step(const rc_state_t* rc_state, uint32_t now_ms, lights_control_output_t* out_state)
{
  const bool l1_pressed = rc_button_is_pressed(rc_state, BUTTON_L1);
  const bool r1_pressed = rc_button_is_pressed(rc_state, BUTTON_R1);
  const bool l1_rising = l1_pressed && !s_lights.prev_l1;
  const bool r1_rising = r1_pressed && !s_lights.prev_r1;
  const bool l1_falling = !l1_pressed && s_lights.prev_l1;
  const bool r1_falling = !r1_pressed && s_lights.prev_r1;
  bool shift_combo_edge = false;
  bool brake_light_on = brake_is_active(rc_state);
  bool main_light_effective_on = s_lights.main_light_on;

  if (out_state == 0)
  {
    return;
  }

  if (s_lights.sync_buttons_on_next_step)
  {
    s_lights.prev_l1 = l1_pressed;
    s_lights.prev_r1 = r1_pressed;
    s_lights.sync_buttons_on_next_step = false;
  }

  if (!l1_pressed && !r1_pressed)
  {
    s_lights.combo_latched = false;
  }

  if (l1_rising)
  {
    s_lights.l1_pending = true;
    s_lights.l1_pressed_ms = now_ms;
  }

  if (r1_rising)
  {
    s_lights.r1_pending = true;
    s_lights.r1_pressed_ms = now_ms;
  }

  if (!s_lights.combo_latched && s_lights.l1_pending && s_lights.r1_pending)
  {
    const uint32_t delta_ms =
        (s_lights.l1_pressed_ms >= s_lights.r1_pressed_ms)
            ? (s_lights.l1_pressed_ms - s_lights.r1_pressed_ms)
            : (s_lights.r1_pressed_ms - s_lights.l1_pressed_ms);

    if (delta_ms <= LIGHTS_COMBO_WINDOW_MS)
    {
      shift_combo_edge = true;
      s_lights.combo_latched = true;
      s_lights.l1_pending = false;
      s_lights.r1_pending = false;
    }
  }

  if (l1_falling && s_lights.l1_pending && !s_lights.combo_latched)
  {
    s_lights.flash_active = true;
    s_lights.flash_until_ms = now_ms + LIGHTS_FLASH_DURATION_MS;
    s_lights.l1_pending = false;
  }

  if (r1_falling && s_lights.r1_pending && !s_lights.combo_latched)
  {
    s_lights.main_light_on = !s_lights.main_light_on;
    s_lights.r1_pending = false;
  }

  if (s_lights.flash_active)
  {
    if ((int32_t)(now_ms - s_lights.flash_until_ms) >= 0)
    {
      s_lights.flash_active = false;
    }
    else
    {
      main_light_effective_on = !s_lights.main_light_on;
    }
  }

  if (!s_lights.flash_active)
  {
    main_light_effective_on = s_lights.main_light_on;
  }

  set_light_outputs(main_light_effective_on, brake_light_on);

  s_lights.prev_l1 = l1_pressed;
  s_lights.prev_r1 = r1_pressed;

  out_state->shift_combo_edge = shift_combo_edge;
  out_state->main_light_on = s_lights.main_light_on;
  out_state->brake_light_on = brake_light_on;
}
