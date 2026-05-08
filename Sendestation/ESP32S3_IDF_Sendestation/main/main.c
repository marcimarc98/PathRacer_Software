#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_CRSF                   UART_NUM_1
#define CRSF_TX_PIN                 GPIO_NUM_17
#define CRSF_RX_PIN                 GPIO_NUM_18

#define USB_BAUD_INFO               460800U
#define CRSF_BAUD                   420000U
#define SEND_PERIOD_US              4000U
#define STATUS_PERIOD_MS            100U
#define STATUS_VALID_MS             3000U
#define DEBUG_PERIOD_MS             250U

#define CRSF_SYNC_BYTE                    0xC8U
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED 0x16U
#define CRSF_FRAMETYPE_BATTERY_SENSOR     0x08U
#define CRSF_FRAMETYPE_LINK_STATISTICS    0x14U
#define CRSF_FRAMETYPE_FLIGHT_MODE        0x21U
#define CRSF_FRAMETYPE_DEVICE_INFO        0x29U
#define CRSF_MAX_FRAME_SIZE               64U
#define CRSF_MIN_LENGTH_FIELD             2U
#define CRSF_MAX_LENGTH_FIELD             62U

#define CRSF_NUM_CHANNELS      16U
#define CRSF_PAYLOAD_SIZE      22U
#define CRSF_LEN_FIELD         (1U + CRSF_PAYLOAD_SIZE + 1U)
#define CRSF_TOTAL_FRAME_SIZE  (2U + CRSF_LEN_FIELD)

#define CRSF_CHANNEL_VALUE_MIN 192U
#define CRSF_CHANNEL_VALUE_MID 992U
#define CRSF_CHANNEL_VALUE_MAX 1792U

#define HOST_HEADER_1               0xAAU
#define HOST_HEADER_2               0x55U
#define HOST_PACKET_SIZE            11U
#define UNUSED_BUTTON_CHANNEL_VALUE CRSF_CHANNEL_VALUE_MIN

#define STATUS_HEADER_1             0x5AU
#define STATUS_HEADER_2             0xA5U
#define STATUS_PACKET_TYPE          0x31U
#define STATUS_PACKET_SIZE          20U

typedef struct {
    int lenkung_us;
    int gas_us;
    int bremse_us;
    bool button[13];
} steuerdaten_t;

typedef struct {
    bool valid;
    char gear;
    bool sport_mode;
    bool main_light_on;
    bool camera_rear_active;
    uint16_t battery_mv;
    uint8_t battery_percent;
    int16_t battery_temp_c;
    uint8_t uplink_lq;
    uint8_t uplink_rssi;
    int8_t uplink_snr;
    uint8_t downlink_lq;
    uint8_t downlink_rssi;
    int8_t downlink_snr;
    uint8_t rf_profile;
    uint8_t tx_power;
    uint32_t last_update_ms;
    uint32_t last_link_activity_ms;
    uint8_t sequence;
} fahrzeug_status_t;

typedef struct {
    uint32_t host_packets_decoded;
    uint32_t telemetry_bytes_seen;
    uint32_t telemetry_frames_seen;
    uint32_t telemetry_crc_errors;
    uint32_t flight_mode_frames_seen;
    uint32_t battery_frames_seen;
    uint32_t link_stats_frames_seen;
    uint32_t device_info_frames_seen;
    uint8_t last_telemetry_type;
} debug_stats_t;

static steuerdaten_t s_state = {
    .lenkung_us = 1500,
    .gas_us = 1000,
    .bremse_us = 1000,
};

static fahrzeug_status_t s_vehicle_status = {
    .valid = false,
    .gear = 'N',
};

static debug_stats_t s_debug_stats = {0};
static uint16_t s_tx_channels[CRSF_NUM_CHANNELS];
static uint8_t s_tx_frame[CRSF_TOTAL_FRAME_SIZE];
static uint8_t s_host_packet[HOST_PACKET_SIZE];
static uint8_t s_host_idx = 0U;
static uint8_t s_rx_frame[CRSF_MAX_FRAME_SIZE];
static uint8_t s_rx_frame_idx = 0U;
static uint8_t s_rx_expected_total = 0U;

static const uint8_t crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54,
    0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06,
    0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0,
    0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2,
    0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9,
    0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B,
    0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D,
    0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F,
    0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB,
    0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9,
    0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F,
    0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D,
    0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26,
    0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74,
    0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82,
    0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0,
    0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9
};

static uint32_t millis_now(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint8_t crc8(const uint8_t *ptr, uint8_t len)
{
    uint8_t crc = 0U;

    while (len-- != 0U) {
        crc = crc8tab[crc ^ *ptr++];
    }

    return crc;
}

static uint16_t clamp_channel(int value)
{
    if (value < (int)CRSF_CHANNEL_VALUE_MIN) {
        return CRSF_CHANNEL_VALUE_MIN;
    }
    if (value > (int)CRSF_CHANNEL_VALUE_MAX) {
        return CRSF_CHANNEL_VALUE_MAX;
    }
    return (uint16_t)value;
}

static int clamp_us(int value)
{
    if (value < 1000) {
        return 1000;
    }
    if (value > 2000) {
        return 2000;
    }
    return value;
}

static uint16_t us_to_crsf(int us)
{
    int value = ((us - 1500) * 8 / 5) + 992;
    return clamp_channel(value);
}

static uint16_t bool_to_crsf(bool value)
{
    return value ? CRSF_CHANNEL_VALUE_MAX : CRSF_CHANNEL_VALUE_MIN;
}

static uint16_t channel_value_for_button(const steuerdaten_t *state, int button_index)
{
    if ((state == NULL) || (button_index < 0) || (button_index >= 13)) {
        return CRSF_CHANNEL_VALUE_MIN;
    }
    return bool_to_crsf(state->button[button_index]);
}

static int norm_steer_to_us(int16_t steer)
{
    return clamp_us(1500 + ((int)steer / 2));
}

static int norm_pedal_to_us(uint16_t pedal)
{
    return clamp_us(1000 + (int)pedal);
}

static void fill_channels(uint16_t *channels)
{
    for (uint32_t i = 0; i < CRSF_NUM_CHANNELS; i++) {
        channels[i] = CRSF_CHANNEL_VALUE_MID;
    }

    channels[0] = us_to_crsf(s_state.lenkung_us);
    channels[1] = us_to_crsf(s_state.gas_us);
    channels[2] = us_to_crsf(s_state.bremse_us);
    channels[3] = channel_value_for_button(&s_state, 0);
    channels[4] = channel_value_for_button(&s_state, 1);
    channels[5] = channel_value_for_button(&s_state, 8);
    channels[6] = channel_value_for_button(&s_state, 9);
    channels[7] = channel_value_for_button(&s_state, 10);
    channels[8] = channel_value_for_button(&s_state, 11);
    channels[9] = channel_value_for_button(&s_state, 12);

    for (uint32_t i = 10; i < CRSF_NUM_CHANNELS; i++) {
        channels[i] = UNUSED_BUTTON_CHANNEL_VALUE;
    }
}

static void build_crsf_rc_frame(uint8_t *frame, const uint16_t *channels)
{
    uint32_t bitbuf = 0U;
    uint8_t bitcnt = 0U;
    uint32_t payload_index = 0U;

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = CRSF_LEN_FIELD;
    frame[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memset(&frame[3], 0, CRSF_PAYLOAD_SIZE);

    for (uint32_t i = 0; i < CRSF_NUM_CHANNELS; i++) {
        const uint16_t value = clamp_channel((int)channels[i]) & 0x07FFU;
        bitbuf |= ((uint32_t)value) << bitcnt;
        bitcnt += 11U;

        while (bitcnt >= 8U) {
            if (payload_index < CRSF_PAYLOAD_SIZE) {
                frame[3U + payload_index] = (uint8_t)(bitbuf & 0xFFU);
            }
            payload_index++;
            bitbuf >>= 8U;
            bitcnt -= 8U;
        }
    }

    if ((payload_index < CRSF_PAYLOAD_SIZE) && (bitcnt > 0U)) {
        frame[3U + payload_index] = (uint8_t)(bitbuf & 0xFFU);
    }

    frame[CRSF_TOTAL_FRAME_SIZE - 1U] = crc8(&frame[2], (uint8_t)(1U + CRSF_PAYLOAD_SIZE));
}

static bool is_valid_sync_byte(uint8_t value)
{
    switch (value) {
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

static void reset_telemetry_parser(void)
{
    s_rx_frame_idx = 0U;
    s_rx_expected_total = 0U;
}

static void parse_flight_mode_payload(const uint8_t *payload, size_t payload_length)
{
    char mode_text[32];
    char gear = 'N';
    char drive_mode[8] = {0};
    unsigned int light = 0U;
    unsigned int camera = 0U;
    int battery_temp_c = 0;
    size_t copy_len = payload_length;

    if (copy_len >= sizeof(mode_text)) {
        copy_len = sizeof(mode_text) - 1U;
    }

    memcpy(mode_text, payload, copy_len);
    mode_text[copy_len] = '\0';

    if (sscanf(mode_text, "%c|%7[^|]|L%u|C%u|T%d", &gear, drive_mode, &light, &camera, &battery_temp_c) == 5) {
        s_vehicle_status.valid = true;
        s_vehicle_status.gear = gear;
        s_vehicle_status.sport_mode = strcmp(drive_mode, "SPORT") == 0;
        s_vehicle_status.main_light_on = (light != 0U);
        s_vehicle_status.camera_rear_active = (camera != 0U);
        s_vehicle_status.battery_temp_c = (int16_t)battery_temp_c;
        s_vehicle_status.last_update_ms = millis_now();
        s_vehicle_status.last_link_activity_ms = millis_now();
    }
}

static void parse_battery_payload(const uint8_t *payload, size_t payload_length)
{
    if (payload_length < 8U) {
        return;
    }

    s_vehicle_status.battery_mv = (uint16_t)((((uint16_t)payload[0] << 8U) | payload[1]) * 10U);
    s_vehicle_status.battery_percent = payload[7];
    s_vehicle_status.last_update_ms = millis_now();
    s_vehicle_status.last_link_activity_ms = millis_now();
}

static void parse_link_statistics_payload(const uint8_t *payload, size_t payload_length)
{
    if (payload_length < 10U) {
        return;
    }

    s_vehicle_status.uplink_rssi = payload[0];
    s_vehicle_status.uplink_lq = payload[2];
    s_vehicle_status.uplink_snr = (int8_t)payload[3];
    s_vehicle_status.rf_profile = payload[5];
    s_vehicle_status.tx_power = payload[6];
    s_vehicle_status.downlink_rssi = payload[7];
    s_vehicle_status.downlink_lq = payload[8];
    s_vehicle_status.downlink_snr = (int8_t)payload[9];
    s_vehicle_status.last_link_activity_ms = millis_now();
}

static void process_telemetry_frame(const uint8_t *frame)
{
    const uint8_t length = frame[1];
    const uint8_t type = frame[2];
    const uint8_t received_crc = frame[1U + length];
    const uint8_t calculated_crc = crc8(&frame[2], (uint8_t)(length - 1U));
    const uint8_t *payload = &frame[3];
    const size_t payload_length = (size_t)(length - 2U);

    s_debug_stats.telemetry_frames_seen++;
    s_debug_stats.last_telemetry_type = type;

    if (received_crc != calculated_crc) {
        s_debug_stats.telemetry_crc_errors++;
        return;
    }

    if (type == CRSF_FRAMETYPE_FLIGHT_MODE) {
        s_debug_stats.flight_mode_frames_seen++;
        parse_flight_mode_payload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_BATTERY_SENSOR) {
        s_debug_stats.battery_frames_seen++;
        parse_battery_payload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_LINK_STATISTICS) {
        s_debug_stats.link_stats_frames_seen++;
        parse_link_statistics_payload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_DEVICE_INFO) {
        s_debug_stats.device_info_frames_seen++;
        s_vehicle_status.last_link_activity_ms = millis_now();
    }
}

static void process_telemetry_byte(uint8_t value)
{
    if (s_rx_frame_idx == 0U) {
        if (!is_valid_sync_byte(value)) {
            return;
        }

        s_rx_frame[s_rx_frame_idx++] = value;
        return;
    }

    if (s_rx_frame_idx == 1U) {
        if ((value < CRSF_MIN_LENGTH_FIELD) || (value > CRSF_MAX_LENGTH_FIELD)) {
            if (is_valid_sync_byte(value)) {
                s_rx_frame[0] = value;
                s_rx_frame_idx = 1U;
                s_rx_expected_total = 0U;
            } else {
                reset_telemetry_parser();
            }
            return;
        }

        s_rx_frame[s_rx_frame_idx++] = value;
        s_rx_expected_total = (uint8_t)(value + 2U);

        if (s_rx_expected_total > CRSF_MAX_FRAME_SIZE) {
            reset_telemetry_parser();
        }
        return;
    }

    s_rx_frame[s_rx_frame_idx++] = value;

    if (s_rx_frame_idx == s_rx_expected_total) {
        process_telemetry_frame(s_rx_frame);
        reset_telemetry_parser();
        return;
    }

    if (s_rx_frame_idx >= CRSF_MAX_FRAME_SIZE) {
        reset_telemetry_parser();
    }
}

static void read_crsf_telemetry(void)
{
    uint8_t buffer[64];
    int bytes_read = uart_read_bytes(UART_CRSF, buffer, sizeof(buffer), 0U);

    if (bytes_read <= 0) {
        return;
    }

    s_debug_stats.telemetry_bytes_seen += (uint32_t)bytes_read;

    for (int i = 0; i < bytes_read; i++) {
        process_telemetry_byte(buffer[i]);
    }
}

static uint8_t gear_to_code(char gear)
{
    switch (gear) {
        case 'N': return 1U;
        case 'D': return 2U;
        case 'R': return 3U;
        default: return 0U;
    }
}

static void usb_write_all(const void *data, size_t length)
{
    const uint8_t *ptr = (const uint8_t *)data;
    size_t remaining = length;

    while (remaining > 0U) {
        int written = usb_serial_jtag_write_bytes(ptr, remaining, 0U);
        if (written <= 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        ptr += written;
        remaining -= (size_t)written;
    }
}

static void send_vehicle_status_to_host(void)
{
    static uint32_t last_status_ms = 0U;
    uint8_t packet[STATUS_PACKET_SIZE];
    uint8_t checksum = 0U;
    uint8_t flags = 0U;
    uint32_t now_ms = millis_now();
    bool link_active = (now_ms - s_vehicle_status.last_link_activity_ms) <= STATUS_VALID_MS;

    if ((now_ms - last_status_ms) < STATUS_PERIOD_MS) {
        return;
    }

    last_status_ms = now_ms;

    if (link_active) {
        flags |= 0x01U;
    }
    if (s_vehicle_status.sport_mode) {
        flags |= 0x02U;
    }
    if (s_vehicle_status.main_light_on) {
        flags |= 0x04U;
    }
    if (s_vehicle_status.camera_rear_active) {
        flags |= 0x10U;
    }

    packet[0] = STATUS_HEADER_1;
    packet[1] = STATUS_HEADER_2;
    packet[2] = STATUS_PACKET_TYPE;
    packet[3] = flags;
    packet[4] = gear_to_code(s_vehicle_status.gear);
    packet[5] = s_vehicle_status.battery_percent;
    packet[6] = (uint8_t)(s_vehicle_status.battery_mv & 0xFFU);
    packet[7] = (uint8_t)((s_vehicle_status.battery_mv >> 8U) & 0xFFU);
    packet[8] = (uint8_t)(s_vehicle_status.battery_temp_c & 0xFFU);
    packet[9] = (uint8_t)((s_vehicle_status.battery_temp_c >> 8U) & 0xFFU);
    packet[10] = s_vehicle_status.uplink_lq;
    packet[11] = s_vehicle_status.uplink_rssi;
    packet[12] = (uint8_t)s_vehicle_status.uplink_snr;
    packet[13] = s_vehicle_status.downlink_lq;
    packet[14] = s_vehicle_status.downlink_rssi;
    packet[15] = (uint8_t)s_vehicle_status.downlink_snr;
    packet[16] = s_vehicle_status.rf_profile;
    packet[17] = s_vehicle_status.tx_power;
    packet[18] = s_vehicle_status.sequence++;

    for (uint32_t i = 0; i < STATUS_PACKET_SIZE - 1U; i++) {
        checksum ^= packet[i];
    }
    packet[19] = checksum;

    usb_write_all(packet, sizeof(packet));
}

static void send_usb_debug_line(void)
{
    static uint32_t last_debug_ms = 0U;
    char line[200];
    uint32_t now_ms = millis_now();

    if ((now_ms - last_debug_ms) < DEBUG_PERIOD_MS) {
        return;
    }

    last_debug_ms = now_ms;

    snprintf(
        line,
        sizeof(line),
        "!dbg host=%lu rx=%lu frm=%lu crc=%lu fm=%lu bat=%lu ls=%lu dev=%lu last=%02X valid=%u gear=%c mv=%u pct=%u tmp=%d\n",
        (unsigned long)s_debug_stats.host_packets_decoded,
        (unsigned long)s_debug_stats.telemetry_bytes_seen,
        (unsigned long)s_debug_stats.telemetry_frames_seen,
        (unsigned long)s_debug_stats.telemetry_crc_errors,
        (unsigned long)s_debug_stats.flight_mode_frames_seen,
        (unsigned long)s_debug_stats.battery_frames_seen,
        (unsigned long)s_debug_stats.link_stats_frames_seen,
        (unsigned long)s_debug_stats.device_info_frames_seen,
        (unsigned int)s_debug_stats.last_telemetry_type,
        s_vehicle_status.valid ? 1U : 0U,
        s_vehicle_status.gear,
        (unsigned int)s_vehicle_status.battery_mv,
        (unsigned int)s_vehicle_status.battery_percent,
        (int)s_vehicle_status.battery_temp_c);

    usb_write_all(line, strlen(line));
}

static bool decode_host_packet(const uint8_t *packet, steuerdaten_t *out_state)
{
    uint8_t checksum = 0U;

    for (uint32_t i = 0; i < HOST_PACKET_SIZE - 1U; i++) {
        checksum ^= packet[i];
    }

    if (checksum != packet[HOST_PACKET_SIZE - 1U]) {
        return false;
    }

    int16_t steer = (int16_t)(packet[2] | (packet[3] << 8U));
    uint16_t throttle = (uint16_t)(packet[4] | (packet[5] << 8U));
    uint16_t brake = (uint16_t)(packet[6] | (packet[7] << 8U));
    uint16_t buttons = (uint16_t)(packet[8] | (packet[9] << 8U));

    out_state->lenkung_us = norm_steer_to_us(steer);
    out_state->gas_us = norm_pedal_to_us(throttle);
    out_state->bremse_us = norm_pedal_to_us(brake);

    for (uint32_t i = 0; i < 13U; i++) {
        out_state->button[i] = ((buttons >> i) & 0x01U) != 0U;
    }

    s_debug_stats.host_packets_decoded++;
    return true;
}

static void reset_host_parser(void)
{
    s_host_idx = 0U;
}

static void read_host_packets(void)
{
    uint8_t buffer[64];
    int bytes_read = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), 0U);

    for (int i = 0; i < bytes_read; i++) {
        uint8_t value = buffer[i];

        if (s_host_idx == 0U) {
            if (value == HOST_HEADER_1) {
                s_host_packet[s_host_idx++] = value;
            }
            continue;
        }

        if (s_host_idx == 1U) {
            if (value == HOST_HEADER_2) {
                s_host_packet[s_host_idx++] = value;
            } else if (value == HOST_HEADER_1) {
                s_host_packet[0] = HOST_HEADER_1;
                s_host_idx = 1U;
            } else {
                reset_host_parser();
            }
            continue;
        }

        s_host_packet[s_host_idx++] = value;

        if (s_host_idx == HOST_PACKET_SIZE) {
            steuerdaten_t next_state;
            if (decode_host_packet(s_host_packet, &next_state)) {
                s_state = next_state;
            }
            reset_host_parser();
        }
    }
}

static void init_usb_serial(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 2048U;
    cfg.rx_buffer_size = 512U;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
}

static void init_crsf_uart(void)
{
    uart_config_t cfg = {
        .baud_rate = (int)CRSF_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_CRSF, 1024, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CRSF, &cfg));
    ESP_ERROR_CHECK(uart_set_mode(UART_CRSF, UART_MODE_UART));
    ESP_ERROR_CHECK(uart_set_pin(UART_CRSF, CRSF_TX_PIN, CRSF_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    gpio_set_pull_mode(CRSF_RX_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(CRSF_RX_PIN, GPIO_MODE_INPUT);

    gpio_set_pull_mode(CRSF_TX_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(CRSF_TX_PIN, GPIO_MODE_INPUT);
}

static void send_crsf_frame(const uint8_t *frame, size_t length)
{
    gpio_set_direction(CRSF_TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CRSF_TX_PIN, 1);

    uart_write_bytes(UART_CRSF, frame, length);
    uart_wait_tx_done(UART_CRSF, pdMS_TO_TICKS(2));

    gpio_set_level(CRSF_TX_PIN, 1);
    gpio_set_direction(CRSF_TX_PIN, GPIO_MODE_INPUT);
}

void app_main(void)
{
    int64_t next_send_us = esp_timer_get_time();

    init_usb_serial();
    init_crsf_uart();
    reset_telemetry_parser();
    reset_host_parser();

    while (1) {
        read_host_packets();
        read_crsf_telemetry();
        send_vehicle_status_to_host();
        send_usb_debug_line();

        int64_t now_us = esp_timer_get_time();
        if (now_us >= next_send_us) {
            next_send_us += SEND_PERIOD_US;
            fill_channels(s_tx_channels);
            build_crsf_rc_frame(s_tx_frame, s_tx_channels);
            send_crsf_frame(s_tx_frame, sizeof(s_tx_frame));
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
