#include "battery_monitor.h"

#include "main.h"

#define ADC_FULL_SCALE_COUNTS            4095U
#define ADC_REFERENCE_MV                 3300U
#define BATTERY_DIVIDER_NUMERATOR        57000U
#define BATTERY_DIVIDER_DENOMINATOR      10000U
#define BATTERY_EMPTY_MV                 13200U
#define BATTERY_FULL_MV                  16800U
#define ADC_STARTUP_TIMEOUT_LOOPS        100000U
#define ADC_CONVERSION_TIMEOUT_LOOPS     100000U

static battery_status_t s_battery_status;

/* Begrenzt berechnete Spannungswerte auf uint16_t. */
static uint16_t clamp_u16(uint32_t value)
{
  return (value > 0xFFFFU) ? 0xFFFFU : (uint16_t)value;
}

/* Einfache SoC-Anzeige fuer 4S Li-Ion zwischen leer und voll. */
static uint8_t battery_mv_to_percent(uint16_t battery_mv)
{
  if (battery_mv <= BATTERY_EMPTY_MV)
  {
    return 0U;
  }

  if (battery_mv >= BATTERY_FULL_MV)
  {
    return 100U;
  }

  return (uint8_t)(((uint32_t)(battery_mv - BATTERY_EMPTY_MV) * 100U) / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));
}

/* Initialisiert den ADC nur fuer die Akkuspannung.
 * Diese Datei ist eine aeltere, einfachere Variante neben sensor_data.c.
 */
void battery_monitor_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t timeout = 0U;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();

  gpio.Pin = BATTERY_ADC_Pin;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BATTERY_ADC_GPIO_Port, &gpio);

  ADC12_COMMON->CCR = ADC12_CCR_CKMODE_0;

  if ((ADC1->CR & ADC_CR_ADVREGEN_0) == 0U)
  {
    ADC1->CR |= ADC_CR_ADVREGEN_0;

    for (volatile uint32_t i = 0U; i < 1000U; i++)
    {
      __NOP();
    }
  }

  if ((ADC1->CR & ADC_CR_ADEN) != 0U)
  {
    ADC1->CR |= ADC_CR_ADDIS;

    while ((ADC1->CR & ADC_CR_ADEN) != 0U)
    {
    }
  }

  ADC1->CR &= ~ADC_CR_ADCALDIF;
  ADC1->CR |= ADC_CR_ADCAL;

  timeout = ADC_STARTUP_TIMEOUT_LOOPS;
  while (((ADC1->CR & ADC_CR_ADCAL) != 0U) && (timeout-- > 0U))
  {
  }

  if ((ADC1->CR & ADC_CR_ADCAL) != 0U)
  {
    Error_Handler();
  }

  ADC1->ISR |= ADC_ISR_ADRDY;
  ADC1->CFGR = 0U;
  ADC1->SQR1 = (1U << ADC_SQR1_SQ1_Pos);
  ADC1->SMPR1 = (6U << ADC_SMPR1_SMP1_Pos);
  ADC1->CR |= ADC_CR_ADEN;

  timeout = ADC_STARTUP_TIMEOUT_LOOPS;
  while (((ADC1->ISR & ADC_ISR_ADRDY) == 0U) && (timeout-- > 0U))
  {
  }

  if ((ADC1->ISR & ADC_ISR_ADRDY) == 0U)
  {
    Error_Handler();
  }

  s_battery_status.battery_mv = 0U;
  s_battery_status.battery_percent = 0U;
}

/* Liest den Akkuspannungskanal und rechnet den Spannungsteiler auf Packspannung hoch. */
void battery_monitor_sample(void)
{
  uint32_t timeout = ADC_CONVERSION_TIMEOUT_LOOPS;
  uint32_t raw_counts = 0U;
  uint32_t sensed_mv = 0U;
  uint32_t battery_mv = 0U;

  ADC1->ISR |= ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR |= ADC_CR_ADSTART;

  while (((ADC1->ISR & ADC_ISR_EOC) == 0U) && (timeout-- > 0U))
  {
  }

  if ((ADC1->ISR & ADC_ISR_EOC) == 0U)
  {
    return;
  }

  raw_counts = ADC1->DR;
  sensed_mv = (raw_counts * ADC_REFERENCE_MV) / ADC_FULL_SCALE_COUNTS;
  battery_mv = (sensed_mv * BATTERY_DIVIDER_NUMERATOR) / BATTERY_DIVIDER_DENOMINATOR;

  s_battery_status.battery_mv = clamp_u16(battery_mv);
  s_battery_status.battery_percent = battery_mv_to_percent(s_battery_status.battery_mv);
}

/* Gibt den zuletzt berechneten Akkustatus aus. */
void battery_monitor_get_status(battery_status_t* out_status)
{
  if (out_status == 0)
  {
    return;
  }

  *out_status = s_battery_status;
}
