#include "crsf_telemetry.h"

#include <stdio.h>
#include <string.h>

#include "crsf_receiver.h"

#define CRSF_DEST_RECEIVER          0xECU
#define CRSF_ORIGIN_FLIGHT_CTRL     0xC8U
#define CRSF_FRAMETYPE_BATTERY      0x08U
#define CRSF_FRAMETYPE_FLIGHT_MODE  0x21U
#define CRSF_MAX_FRAME_SIZE         64U
#define TELEMETRY_PERIOD_MS         250U
#define BATTERY_PERIOD_MS           5000U

static uint32_t s_last_telemetry_ms = 0U;
static uint32_t s_last_battery_ms = 0U;

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

static char gear_to_char(vehicle_gear_t gear)
{
  switch (gear)
  {
    case VEHICLE_GEAR_REVERSE:
      return 'R';
    case VEHICLE_GEAR_DRIVE:
      return 'D';
    case VEHICLE_GEAR_NEUTRAL:
    default:
      return 'N';
  }
}

static const char* drive_mode_to_text(vehicle_drive_mode_t drive_mode)
{
  return (drive_mode == VEHICLE_DRIVE_MODE_SPORT) ? "SPORT" : "NORMAL";
}

static uint8_t build_flight_mode_frame(uint8_t* out_frame, size_t out_frame_size, const crsf_vehicle_status_t* status)
{
  char payload[24];
  size_t payload_length = 0U;
  uint8_t frame_length = 0U;

  if ((out_frame == 0) || (status == 0) || (out_frame_size < 8U))
  {
    return 0U;
  }

  (void)snprintf(
      payload,
      sizeof(payload),
      "%c|%s|L%u|C%u",
      gear_to_char(status->gear),
      drive_mode_to_text(status->drive_mode),
      status->main_light_on ? 1U : 0U,
      status->camera_rear_active ? 1U : 0U);

  payload_length = strlen(payload) + 1U;

  if ((payload_length + 4U) > out_frame_size)
  {
    return 0U;
  }

  frame_length = (uint8_t)(payload_length + 2U);

  out_frame[0] = CRSF_DEST_RECEIVER;
  out_frame[1] = frame_length;
  out_frame[2] = CRSF_FRAMETYPE_FLIGHT_MODE;
  memcpy(&out_frame[3], payload, payload_length);
  out_frame[3U + payload_length] = crc8(&out_frame[2], (uint8_t)(payload_length + 1U));

  return (uint8_t)(payload_length + 4U);
}

static uint8_t build_battery_frame(uint8_t* out_frame, size_t out_frame_size, const crsf_vehicle_status_t* status)
{
  if ((out_frame == 0) || (status == 0) || (out_frame_size < 12U))
  {
    return 0U;
  }

  out_frame[0] = CRSF_DEST_RECEIVER;
  out_frame[1] = 10U;
  out_frame[2] = CRSF_FRAMETYPE_BATTERY;
  out_frame[3] = (uint8_t)(((status->battery_mv / 10U) >> 8U) & 0xFFU);
  out_frame[4] = (uint8_t)((status->battery_mv / 10U) & 0xFFU);
  out_frame[5] = 0U;
  out_frame[6] = 0U;
  out_frame[7] = 0U;
  out_frame[8] = 0U;
  out_frame[9] = 0U;
  out_frame[10] = status->battery_percent;
  out_frame[11] = crc8(&out_frame[2], 9U);

  return 12U;
}

void crsf_telemetry_init(void)
{
  s_last_telemetry_ms = 0U;
  s_last_battery_ms = 0U;
}

void crsf_telemetry_process(uint32_t now_ms, const crsf_vehicle_status_t* status)
{
  uint8_t frame[CRSF_MAX_FRAME_SIZE];
  uint8_t frame_size = 0U;

  if (status == 0)
  {
    return;
  }

  if ((s_last_telemetry_ms == 0U) || ((now_ms - s_last_telemetry_ms) >= TELEMETRY_PERIOD_MS))
  {
    frame_size = build_flight_mode_frame(frame, sizeof(frame), status);

    if ((frame_size != 0U) && crsf_receiver_send_frame(frame, frame_size))
    {
      s_last_telemetry_ms = now_ms;
    }
  }

  if ((s_last_battery_ms == 0U) || ((now_ms - s_last_battery_ms) >= BATTERY_PERIOD_MS))
  {
    frame_size = build_battery_frame(frame, sizeof(frame), status);

    if ((frame_size != 0U) && crsf_receiver_send_frame(frame, frame_size))
    {
      s_last_battery_ms = now_ms;
    }
  }

}
