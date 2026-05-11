#include "drive_pwm.h"

#include "main.h"

#define PWM_TIMER_TICK_HZ         1000000U
#define PWM_PERIOD_TICKS          20000U
#define SERVO_PWM_CHANNEL_COMPARE TIM3->CCR1
#define ESC_PWM_CHANNEL_COMPARE   TIM3->CCR2
#define CAMERA_PWM_CHANNEL_COMPARE TIM2->CCR1
#define CAMERA_PWM_FRONT_US       1100U
#define CAMERA_PWM_REAR_US        1500U

static bool s_camera_rear_active = false;

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

static uint32_t get_apb_timer_clock_hz(void)
{
  const uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();
  const uint32_t ppre1 = RCC->CFGR & RCC_CFGR_PPRE1;

  if (ppre1 == RCC_CFGR_PPRE1_DIV1)
  {
    return pclk1_hz;
  }

  return pclk1_hz * 2U;
}

static void write_camera_pwm_output(bool camera_rear_active)
{
  s_camera_rear_active = camera_rear_active;
  CAMERA_PWM_CHANNEL_COMPARE = camera_rear_active ? CAMERA_PWM_REAR_US : CAMERA_PWM_FRONT_US;
}

static void write_pwm_outputs(int servo_us, int esc_us, bool camera_rear_active)
{
  SERVO_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(servo_us);
  ESC_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(esc_us);
  write_camera_pwm_output(camera_rear_active);
}

void drive_pwm_init(void)
{
  GPIO_InitTypeDef gpio_tim3 = {0};
  GPIO_InitTypeDef gpio_tim2 = {0};
  uint32_t timer_clock_hz = get_apb_timer_clock_hz();
  uint32_t prescaler = 0U;

  if (timer_clock_hz < PWM_TIMER_TICK_HZ)
  {
    Error_Handler();
  }

  prescaler = (timer_clock_hz / PWM_TIMER_TICK_HZ) - 1U;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();

  gpio_tim3.Pin = SERVO_PWM_Pin | ESC_PWM_Pin;
  gpio_tim3.Mode = GPIO_MODE_AF_PP;
  gpio_tim3.Pull = GPIO_NOPULL;
  gpio_tim3.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_tim3.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &gpio_tim3);

  gpio_tim2.Pin = CAMERA_PWM_Pin;
  gpio_tim2.Mode = GPIO_MODE_AF_PP;
  gpio_tim2.Pull = GPIO_NOPULL;
  gpio_tim2.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_tim2.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &gpio_tim2);

  TIM3->CR1 = 0U;
  TIM3->PSC = prescaler;
  TIM3->ARR = PWM_PERIOD_TICKS - 1U;
  TIM3->CCR1 = 1500U;
  TIM3->CCR2 = 1500U;
  TIM3->CCMR1 =
      TIM_CCMR1_OC1PE |
      (6U << TIM_CCMR1_OC1M_Pos) |
      TIM_CCMR1_OC2PE |
      (6U << TIM_CCMR1_OC2M_Pos);
  TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  TIM2->CR1 = 0U;
  TIM2->PSC = prescaler;
  TIM2->ARR = PWM_PERIOD_TICKS - 1U;
  TIM2->CCR1 = CAMERA_PWM_FRONT_US;
  TIM2->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
  TIM2->CCER = TIM_CCER_CC1E;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  write_pwm_outputs(1500, 1500, false);
}

void drive_pwm_apply(int servo_us, int esc_us, bool camera_rear_active)
{
  write_pwm_outputs(servo_us, esc_us, camera_rear_active);
}

void drive_pwm_apply_failsafe(void)
{
  write_pwm_outputs(1500, 1500, s_camera_rear_active);
}
