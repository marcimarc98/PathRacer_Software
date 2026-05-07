#include "drive_pwm.h"

#include "main.h"

#define PWM_TIMER_TICK_HZ         1000000U
#define PWM_PERIOD_TICKS          20000U
#define SERVO_PWM_CHANNEL_COMPARE TIM3->CCR1
#define ESC_PWM_CHANNEL_COMPARE   TIM3->CCR2

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

static void write_pwm_outputs(int servo_us, int esc_us)
{
  SERVO_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(servo_us);
  ESC_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(esc_us);
}

static uint32_t get_tim3_clock_hz(void)
{
  const uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();
  const uint32_t ppre1 = RCC->CFGR & RCC_CFGR_PPRE1;

  if (ppre1 == RCC_CFGR_PPRE1_DIV1)
  {
    return pclk1_hz;
  }

  return pclk1_hz * 2U;
}

void drive_pwm_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t timer_clock_hz = get_tim3_clock_hz();
  uint32_t prescaler = 0U;

  if (timer_clock_hz < PWM_TIMER_TICK_HZ)
  {
    Error_Handler();
  }

  prescaler = (timer_clock_hz / PWM_TIMER_TICK_HZ) - 1U;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();

  gpio.Pin = SERVO_PWM_Pin | ESC_PWM_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &gpio);

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

  write_pwm_outputs(1500, 1500);
}

void drive_pwm_apply(int servo_us, int esc_us)
{
  write_pwm_outputs(servo_us, esc_us);
}

void drive_pwm_apply_failsafe(void)
{
  write_pwm_outputs(1500, 1500);
}
