#include "drive_pwm.h"

#include "main.h"

#define PWM_TIMER_TICK_HZ         1000000U
#define PWM_PERIOD_TICKS          20000U
#define SERVO_PWM_CHANNEL_COMPARE TIM3->CCR1
#define ESC_PWM_CHANNEL_COMPARE   TIM3->CCR2
#define DIFF_FRONT_PWM_CHANNEL_COMPARE TIM3->CCR3
#define DIFF_REAR_PWM_CHANNEL_COMPARE  TIM3->CCR4
#define CAMERA_SWITCH_PWM_CHANNEL_COMPARE TIM2->CCR1
#define CAMERA_PAN_PWM_CHANNEL_COMPARE TIM1->CCR1
#define CAMERA_SWITCH_FRONT_US    1100U
#define CAMERA_SWITCH_REAR_US     1500U
#define CAMERA_PAN_MIN_DEG        (-90)
#define CAMERA_PAN_MAX_DEG        90
#define CAMERA_PAN_CENTER_US      1500
#define CAMERA_PAN_RANGE_US       1000
#define DIFF_UNLOCKED_US          1000U
#define DIFF_LOCKED_US            2000U

static bool s_camera_rear_active = false;
static int s_camera_pan_angle_deg = 0;
static bool s_diff_front_locked = false;
static bool s_diff_rear_locked = false;

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

static int clamp_camera_angle_deg(int angle_deg)
{
  if (angle_deg < CAMERA_PAN_MIN_DEG)
  {
    return CAMERA_PAN_MIN_DEG;
  }

  if (angle_deg > CAMERA_PAN_MAX_DEG)
  {
    return CAMERA_PAN_MAX_DEG;
  }

  return angle_deg;
}

static uint32_t camera_angle_to_us(int angle_deg)
{
  const int clamped_angle_deg = clamp_camera_angle_deg(angle_deg);
  return (uint32_t)(CAMERA_PAN_CENTER_US - ((clamped_angle_deg * CAMERA_PAN_RANGE_US) / CAMERA_PAN_MAX_DEG));
}

static uint32_t diff_state_to_us(bool locked)
{
  return locked ? DIFF_LOCKED_US : DIFF_UNLOCKED_US;
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

static uint32_t get_apb2_timer_clock_hz(void)
{
  const uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();
  const uint32_t ppre2 = RCC->CFGR & RCC_CFGR_PPRE2;

  if (ppre2 == RCC_CFGR_PPRE2_DIV1)
  {
    return pclk2_hz;
  }

  return pclk2_hz * 2U;
}

static void write_camera_pwm_output(bool camera_rear_active)
{
  s_camera_rear_active = camera_rear_active;
  CAMERA_SWITCH_PWM_CHANNEL_COMPARE = camera_rear_active ? CAMERA_SWITCH_REAR_US : CAMERA_SWITCH_FRONT_US;
}

static void write_camera_pan_pwm_output(int camera_pan_angle_deg)
{
  s_camera_pan_angle_deg = clamp_camera_angle_deg(camera_pan_angle_deg);
  CAMERA_PAN_PWM_CHANNEL_COMPARE = camera_angle_to_us(s_camera_pan_angle_deg);
}

static void write_diff_pwm_outputs(bool diff_front_locked, bool diff_rear_locked)
{
  s_diff_front_locked = diff_front_locked;
  s_diff_rear_locked = diff_rear_locked;
  DIFF_FRONT_PWM_CHANNEL_COMPARE = diff_state_to_us(s_diff_front_locked);
  DIFF_REAR_PWM_CHANNEL_COMPARE = diff_state_to_us(s_diff_rear_locked);
}

static void write_pwm_outputs(
    int servo_us,
    int esc_us,
    bool camera_rear_active,
    int camera_pan_angle_deg,
    bool diff_front_locked,
    bool diff_rear_locked)
{
  SERVO_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(servo_us);
  ESC_PWM_CHANNEL_COMPARE = (uint32_t)clamp_us(esc_us);
  write_camera_pwm_output(camera_rear_active);
  write_camera_pan_pwm_output(camera_pan_angle_deg);
  write_diff_pwm_outputs(diff_front_locked, diff_rear_locked);
}

void drive_pwm_init(void)
{
  GPIO_InitTypeDef gpio_tim3 = {0};
  GPIO_InitTypeDef gpio_tim2 = {0};
  GPIO_InitTypeDef gpio_tim1 = {0};
  uint32_t apb1_timer_clock_hz = get_apb_timer_clock_hz();
  uint32_t apb2_timer_clock_hz = get_apb2_timer_clock_hz();
  uint32_t apb1_prescaler = 0U;
  uint32_t apb2_prescaler = 0U;

  if ((apb1_timer_clock_hz < PWM_TIMER_TICK_HZ) || (apb2_timer_clock_hz < PWM_TIMER_TICK_HZ))
  {
    Error_Handler();
  }

  apb1_prescaler = (apb1_timer_clock_hz / PWM_TIMER_TICK_HZ) - 1U;
  apb2_prescaler = (apb2_timer_clock_hz / PWM_TIMER_TICK_HZ) - 1U;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  gpio_tim3.Pin = SERVO_PWM_Pin | ESC_PWM_Pin;
  gpio_tim3.Mode = GPIO_MODE_AF_PP;
  gpio_tim3.Pull = GPIO_NOPULL;
  gpio_tim3.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_tim3.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &gpio_tim3);

  gpio_tim3.Pin = DIFF_FRONT_PWM_Pin | DIFF_REAR_PWM_Pin;
  HAL_GPIO_Init(GPIOB, &gpio_tim3);

  gpio_tim2.Pin = CAMERA_PWM_Pin;
  gpio_tim2.Mode = GPIO_MODE_AF_PP;
  gpio_tim2.Pull = GPIO_NOPULL;
  gpio_tim2.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_tim2.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &gpio_tim2);

  gpio_tim1.Pin = CAMERA_PAN_PWM_Pin;
  gpio_tim1.Mode = GPIO_MODE_AF_PP;
  gpio_tim1.Pull = GPIO_NOPULL;
  gpio_tim1.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_tim1.Alternate = GPIO_AF6_TIM1;
  HAL_GPIO_Init(GPIOA, &gpio_tim1);

  TIM3->CR1 = 0U;
  TIM3->PSC = apb1_prescaler;
  TIM3->ARR = PWM_PERIOD_TICKS - 1U;
  TIM3->CCR1 = 1500U;
  TIM3->CCR2 = 1500U;
  TIM3->CCR3 = DIFF_UNLOCKED_US;
  TIM3->CCR4 = DIFF_UNLOCKED_US;
  TIM3->CCMR1 =
      TIM_CCMR1_OC1PE |
      (6U << TIM_CCMR1_OC1M_Pos) |
      TIM_CCMR1_OC2PE |
      (6U << TIM_CCMR1_OC2M_Pos);
  TIM3->CCMR2 =
      TIM_CCMR2_OC3PE |
      (6U << TIM_CCMR2_OC3M_Pos) |
      TIM_CCMR2_OC4PE |
      (6U << TIM_CCMR2_OC4M_Pos);
  TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  TIM2->CR1 = 0U;
  TIM2->PSC = apb1_prescaler;
  TIM2->ARR = PWM_PERIOD_TICKS - 1U;
  TIM2->CCR1 = CAMERA_SWITCH_FRONT_US;
  TIM2->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
  TIM2->CCER = TIM_CCER_CC1E;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  TIM1->CR1 = 0U;
  TIM1->PSC = apb2_prescaler;
  TIM1->ARR = PWM_PERIOD_TICKS - 1U;
  TIM1->CCR1 = camera_angle_to_us(0);
  TIM1->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
  TIM1->CCER = TIM_CCER_CC1E;
  TIM1->BDTR = TIM_BDTR_MOE;
  TIM1->EGR = TIM_EGR_UG;
  TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

  write_pwm_outputs(1500, 1500, false, 0, false, false);
}

void drive_pwm_apply(
    int servo_us,
    int esc_us,
    bool camera_rear_active,
    int camera_pan_angle_deg,
    bool diff_front_locked,
    bool diff_rear_locked)
{
  write_pwm_outputs(servo_us, esc_us, camera_rear_active, camera_pan_angle_deg, diff_front_locked, diff_rear_locked);
}

void drive_pwm_apply_failsafe(void)
{
  write_pwm_outputs(
      1500,
      1500,
      s_camera_rear_active,
      s_camera_pan_angle_deg,
      s_diff_front_locked,
      s_diff_rear_locked);
}
