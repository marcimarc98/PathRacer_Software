#include "sensor_data.h"

#include <math.h>

#include "main.h"

#define ADC_FULL_SCALE_COUNTS            4095U
#define ADC_REFERENCE_MV                 3300U
#define ADC_STARTUP_TIMEOUT_LOOPS        100000U
#define ADC_CONVERSION_TIMEOUT_LOOPS     100000U

#define BATTERY_DIVIDER_TOP_OHM          47000U
#define BATTERY_DIVIDER_BOTTOM_OHM       10000U
#define BATTERY_EMPTY_MV                 13200U
#define BATTERY_FULL_MV                  16800U
#define BATTERY_FILTER_WINDOW_SAMPLES    32U

#define BATTERY_TEMP_DIVIDER_PULLUP_OHM  3900.0f
#define BATTERY_TEMP_NTC_R25_OHM         10000.0f
#define BATTERY_TEMP_NTC_BETA            3950.0f
#define BATTERY_TEMP_ROOM_KELVIN         298.15f
#define BATTERY_TEMP_MIN_C               (-40)
#define BATTERY_TEMP_MAX_C               125
#define BATTERY_TEMP_LOOKUP_MIN_C        20
#define BATTERY_TEMP_LOOKUP_MAX_C        80

#define BATTERY_ADC_CHANNEL_INDEX        4U
#define BATTERY_TEMP_ADC_CHANNEL_INDEX   2U

static sensor_data_status_t s_sensor_status;
static uint32_t s_battery_raw_accumulator = 0U;
static uint32_t s_battery_raw_sample_count = 0U;
static const uint16_t s_battery_temp_ntc_ohm_lookup[] = {
    12501U, 11949U, 11424U, 10925U, 10451U, 10000U, 9571U,  9163U,  8774U,  8404U,
    8051U,  7715U,  7395U,  7090U,  6800U,  6522U,  6258U,  6005U,  5764U,  5534U,
    5315U,  5105U,  4905U,  4713U,  4530U,  4355U,  4188U,  4028U,  3875U,  3728U,
    3588U,  3454U,  3326U,  3203U,  3085U,  2972U,  2864U,  2760U,  2660U,  2565U,
    2473U,  2386U,  2301U,  2221U,  2143U,  2069U,  1997U,  1928U,  1862U,  1799U,
    1738U,  1679U,  1623U,  1569U,  1517U,  1467U,  1419U,  1372U,  1328U,  1285U,
    1243U
};

static uint16_t clamp_u16(uint32_t value)
{
  return (value > 0xFFFFU) ? 0xFFFFU : (uint16_t)value;
}

static int16_t clamp_i16(int32_t value)
{
  if (value < -32768)
  {
    return -32768;
  }

  if (value > 32767)
  {
    return 32767;
  }

  return (int16_t)value;
}

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

static uint16_t adc_read_channel(uint32_t channel_index)
{
  uint32_t timeout = ADC_CONVERSION_TIMEOUT_LOOPS;

  ADC1->SQR1 = (channel_index << ADC_SQR1_SQ1_Pos);
  ADC1->ISR |= ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR |= ADC_CR_ADSTART;

  while (((ADC1->ISR & ADC_ISR_EOC) == 0U) && (timeout-- > 0U))
  {
  }

  if ((ADC1->ISR & ADC_ISR_EOC) == 0U)
  {
    return 0U;
  }

  return (uint16_t)ADC1->DR;
}

static uint16_t adc_counts_to_mv(uint16_t raw_counts)
{
  return (uint16_t)(((uint32_t)raw_counts * ADC_REFERENCE_MV) / ADC_FULL_SCALE_COUNTS);
}

static int battery_temp_lookup_c_from_resistance(float ntc_resistance, float* out_temperature_celsius)
{
  const size_t last_index = (sizeof(s_battery_temp_ntc_ohm_lookup) / sizeof(s_battery_temp_ntc_ohm_lookup[0])) - 1U;

  if ((out_temperature_celsius == 0) ||
      (ntc_resistance > (float)s_battery_temp_ntc_ohm_lookup[0]) ||
      (ntc_resistance < (float)s_battery_temp_ntc_ohm_lookup[last_index]))
  {
    return 0;
  }

  for (size_t index = 0U; index < last_index; index++)
  {
    const float high_ohm = (float)s_battery_temp_ntc_ohm_lookup[index];
    const float low_ohm = (float)s_battery_temp_ntc_ohm_lookup[index + 1U];

    if ((ntc_resistance <= high_ohm) && (ntc_resistance >= low_ohm))
    {
      float temperature_celsius = (float)(BATTERY_TEMP_LOOKUP_MIN_C + (int32_t)index);

      if (high_ohm > low_ohm)
      {
        temperature_celsius += (high_ohm - ntc_resistance) / (high_ohm - low_ohm);
      }

      *out_temperature_celsius = temperature_celsius;
      return 1;
    }
  }

  *out_temperature_celsius = (float)BATTERY_TEMP_LOOKUP_MAX_C;
  return 1;
}

static uint16_t battery_voltage_mv_from_counts(uint16_t raw_counts)
{
  const uint32_t sensed_mv = adc_counts_to_mv(raw_counts);
  const uint32_t battery_mv =
      (sensed_mv * (BATTERY_DIVIDER_TOP_OHM + BATTERY_DIVIDER_BOTTOM_OHM)) / BATTERY_DIVIDER_BOTTOM_OHM;

  return clamp_u16(battery_mv);
}

static int16_t battery_temp_c_from_counts(uint16_t raw_counts)
{
  const float adc_ratio = (float)raw_counts / (float)ADC_FULL_SCALE_COUNTS;
  float ntc_resistance = 0.0f;
  float temperature_kelvin = 0.0f;
  float temperature_celsius = 0.0f;

  if (raw_counts == 0U)
  {
    return BATTERY_TEMP_MAX_C;
  }

  if (raw_counts >= ADC_FULL_SCALE_COUNTS)
  {
    return BATTERY_TEMP_MIN_C;
  }

  ntc_resistance = (BATTERY_TEMP_DIVIDER_PULLUP_OHM * adc_ratio) / (1.0f - adc_ratio);

  if (ntc_resistance <= 0.0f)
  {
    return 0;
  }

  if (battery_temp_lookup_c_from_resistance(ntc_resistance, &temperature_celsius) == 0)
  {
    temperature_kelvin =
        1.0f /
        ((1.0f / BATTERY_TEMP_ROOM_KELVIN) +
         (logf(ntc_resistance / BATTERY_TEMP_NTC_R25_OHM) / BATTERY_TEMP_NTC_BETA));
    temperature_celsius = temperature_kelvin - 273.15f;
  }

  if (temperature_celsius < (float)BATTERY_TEMP_MIN_C)
  {
    temperature_celsius = (float)BATTERY_TEMP_MIN_C;
  }
  else if (temperature_celsius > (float)BATTERY_TEMP_MAX_C)
  {
    temperature_celsius = (float)BATTERY_TEMP_MAX_C;
  }

  return clamp_i16((int32_t)(temperature_celsius >= 0.0f ? (temperature_celsius + 0.5f) : (temperature_celsius - 0.5f)));
}

void sensor_data_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t timeout = 0U;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();

  gpio.Pin = BATTERY_ADC_Pin | BATTERY_TEMP_ADC_Pin;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

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
  ADC1->SMPR1 =
      (6U << ADC_SMPR1_SMP4_Pos) |
      (6U << ADC_SMPR1_SMP2_Pos);
  ADC1->CR |= ADC_CR_ADEN;

  timeout = ADC_STARTUP_TIMEOUT_LOOPS;
  while (((ADC1->ISR & ADC_ISR_ADRDY) == 0U) && (timeout-- > 0U))
  {
  }

  if ((ADC1->ISR & ADC_ISR_ADRDY) == 0U)
  {
    Error_Handler();
  }

  s_sensor_status.battery_mv = 0U;
  s_sensor_status.battery_percent = 0U;
  s_sensor_status.battery_temp_c = 0;
  s_battery_raw_accumulator = 0U;
  s_battery_raw_sample_count = 0U;
}

void sensor_data_sample(void)
{
  const uint16_t battery_raw = adc_read_channel(BATTERY_ADC_CHANNEL_INDEX);
  const uint16_t battery_temp_raw = adc_read_channel(BATTERY_TEMP_ADC_CHANNEL_INDEX);
  uint16_t filtered_battery_raw = battery_raw;

  s_battery_raw_accumulator += battery_raw;
  s_battery_raw_sample_count++;

  if (s_battery_raw_sample_count >= BATTERY_FILTER_WINDOW_SAMPLES)
  {
    filtered_battery_raw = (uint16_t)(s_battery_raw_accumulator / BATTERY_FILTER_WINDOW_SAMPLES);
    s_battery_raw_accumulator = filtered_battery_raw;
    s_battery_raw_sample_count = 1U;
  }
  else if (s_battery_raw_sample_count > 0U)
  {
    filtered_battery_raw = (uint16_t)(s_battery_raw_accumulator / s_battery_raw_sample_count);
  }

  s_sensor_status.battery_mv = battery_voltage_mv_from_counts(filtered_battery_raw);
  s_sensor_status.battery_percent = battery_mv_to_percent(s_sensor_status.battery_mv);
  s_sensor_status.battery_temp_c = battery_temp_c_from_counts(battery_temp_raw);
}

void sensor_data_get_status(sensor_data_status_t* out_status)
{
  if (out_status == 0)
  {
    return;
  }

  *out_status = s_sensor_status;
}
