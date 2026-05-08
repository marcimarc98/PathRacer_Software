#include "crsf_receiver.h"

#include "main.h"
#include "rc_state.h"

#define CRSF_BAUD                  416666U
#define CRSF_MAX_FRAME_SIZE        64U
#define CRSF_MIN_LENGTH_FIELD      2U
#define CRSF_MAX_LENGTH_FIELD      62U
#define CRSF_FRAMETYPE_RC_CHANNELS 0x16U
#define CRSF_RC_FRAME_LENGTH       24U

static UART_HandleTypeDef s_crsf_uart;
static uint8_t s_frame[CRSF_MAX_FRAME_SIZE];
static uint8_t s_frame_idx = 0U;
static uint8_t s_expected_total = 0U;

static uint8_t crc8(const uint8_t* ptr, uint8_t len)
{
  uint8_t crc = 0U;

  while (len-- != 0U)
  {
    crc ^= *ptr++;

    for (uint8_t i = 0U; i < 8U; i++)
    {
      crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0xD5U) : (uint8_t)(crc << 1U);
    }
  }

  return crc;
}

static bool is_valid_sync_byte(uint8_t value)
{
  switch (value)
  {
    case 0x00U:
    case 0xC8U:
    case 0xEAU:
    case 0xECU:
    case 0xEEU:
      return true;
    default:
      return false;
  }
}

static uint16_t unpack11(const uint8_t* payload, uint32_t channel)
{
  const uint32_t bit_index = channel * 11U;
  const uint32_t byte_index = bit_index >> 3;
  const uint32_t shift = bit_index & 7U;
  uint32_t word = (uint32_t)payload[byte_index];

  if ((byte_index + 1U) < 22U)
  {
    word |= ((uint32_t)payload[byte_index + 1U]) << 8;
  }

  if ((byte_index + 2U) < 22U)
  {
    word |= ((uint32_t)payload[byte_index + 2U]) << 16;
  }

  return (uint16_t)((word >> shift) & 0x07FFU);
}

static void reset_frame_parser(void)
{
  s_frame_idx = 0U;
  s_expected_total = 0U;
}

static void process_frame(const uint8_t* frame)
{
  const uint8_t length = frame[1];
  const uint8_t type = frame[2];
  const uint8_t received_crc = frame[1U + length];
  const uint8_t calculated_crc = crc8(&frame[2], (uint8_t)(length - 1U));
  uint16_t channels[RC_STATE_NUM_CHANNELS];
  uint32_t i = 0U;

  if (calculated_crc != received_crc)
  {
    return;
  }

  if ((type != CRSF_FRAMETYPE_RC_CHANNELS) || (length != CRSF_RC_FRAME_LENGTH))
  {
    return;
  }

  for (i = 0U; i < RC_STATE_NUM_CHANNELS; i++)
  {
    channels[i] = unpack11(&frame[3], i);
  }

  rc_state_update_from_channels(channels, HAL_GetTick());
}

static void process_crsf_byte(uint8_t value)
{
  if (s_frame_idx == 0U)
  {
    if (!is_valid_sync_byte(value))
    {
      return;
    }

    s_frame[s_frame_idx++] = value;
    return;
  }

  if (s_frame_idx == 1U)
  {
    if ((value < CRSF_MIN_LENGTH_FIELD) || (value > CRSF_MAX_LENGTH_FIELD))
    {
      if (is_valid_sync_byte(value))
      {
        s_frame[0] = value;
        s_frame_idx = 1U;
        s_expected_total = 0U;
      }
      else
      {
        reset_frame_parser();
      }

      return;
    }

    s_frame[s_frame_idx++] = value;
    s_expected_total = (uint8_t)(value + 2U);

    if (s_expected_total > CRSF_MAX_FRAME_SIZE)
    {
      reset_frame_parser();
    }

    return;
  }

  s_frame[s_frame_idx++] = value;

  if (s_frame_idx == s_expected_total)
  {
    process_frame(s_frame);
    reset_frame_parser();
    return;
  }

  if (s_frame_idx >= CRSF_MAX_FRAME_SIZE)
  {
    reset_frame_parser();
  }
}

void crsf_receiver_init(void)
{
  s_crsf_uart.Instance = USART1;
  s_crsf_uart.Init.BaudRate = CRSF_BAUD;
  s_crsf_uart.Init.WordLength = UART_WORDLENGTH_8B;
  s_crsf_uart.Init.StopBits = UART_STOPBITS_1;
  s_crsf_uart.Init.Parity = UART_PARITY_NONE;
  s_crsf_uart.Init.Mode = UART_MODE_TX_RX;
  s_crsf_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  s_crsf_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  s_crsf_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  s_crsf_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&s_crsf_uart) != HAL_OK)
  {
    Error_Handler();
  }

  reset_frame_parser();

  __HAL_UART_ENABLE_IT(&s_crsf_uart, UART_IT_RXNE);
  __HAL_UART_ENABLE_IT(&s_crsf_uart, UART_IT_ERR);
  __HAL_UART_ENABLE_IT(&s_crsf_uart, UART_IT_PE);
}

bool crsf_receiver_send_frame(const uint8_t* frame, uint8_t length)
{
  if ((frame == 0) || (length == 0U))
  {
    return false;
  }

  return HAL_UART_Transmit(&s_crsf_uart, (uint8_t*)frame, length, 10U) == HAL_OK;
}

void crsf_receiver_irq_handler(void)
{
  uint32_t isr = s_crsf_uart.Instance->ISR;

  if ((isr & USART_ISR_RXNE) != 0U)
  {
    process_crsf_byte((uint8_t)(s_crsf_uart.Instance->RDR & 0xFFU));
  }

  if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) != 0U)
  {
    reset_frame_parser();

    __HAL_UART_CLEAR_OREFLAG(&s_crsf_uart);
    __HAL_UART_CLEAR_NEFLAG(&s_crsf_uart);
    __HAL_UART_CLEAR_FEFLAG(&s_crsf_uart);
    __HAL_UART_CLEAR_PEFLAG(&s_crsf_uart);
  }
}
