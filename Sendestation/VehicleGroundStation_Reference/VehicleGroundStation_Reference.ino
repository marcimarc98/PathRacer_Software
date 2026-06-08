#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp32-hal-matrix.h"
#include "esp_err.h"

// --------------------------------------------------
// USER CONFIG
// --------------------------------------------------
static constexpr uart_port_t UART_CRSF = UART_NUM_1;
static constexpr int CRSF_DATA_PIN = 17;

static constexpr uint32_t USB_BAUD = 460800;
static constexpr uint32_t CRSF_BAUD = 420000;
static constexpr uint32_t SEND_PERIOD_US = 4000;      // 250 Hz
static constexpr uint32_t STATUS_PERIOD_MS = 100;
static constexpr uint32_t STATUS_VALID_MS = 3000;
static constexpr uint32_t DEBUG_PERIOD_MS = 250;

// --------------------------------------------------
// CRSF DEFINITIONS
// --------------------------------------------------
static constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
static constexpr uint8_t CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
static constexpr uint8_t CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08;
static constexpr uint8_t CRSF_FRAMETYPE_LINK_STATISTICS = 0x14;
static constexpr uint8_t CRSF_FRAMETYPE_FLIGHT_MODE = 0x21;
static constexpr uint8_t CRSF_FRAMETYPE_DEVICE_INFO = 0x29;

static constexpr uint8_t CRSF_MIN_LENGTH_FIELD = 2;
static constexpr uint8_t CRSF_MAX_LENGTH_FIELD = 62;
static constexpr uint8_t CRSF_MAX_FRAME_SIZE = 64;

static constexpr uint8_t CRSF_NUM_CHANNELS = 16;
static constexpr uint8_t CRSF_PAYLOAD_SIZE = 22;
static constexpr uint8_t CRSF_LEN_FIELD = 1 + CRSF_PAYLOAD_SIZE + 1;
static constexpr uint8_t CRSF_TOTAL_FRAME_SIZE = 2 + CRSF_LEN_FIELD;

static constexpr uint16_t CRSF_CHANNEL_VALUE_MIN = 192;
static constexpr uint16_t CRSF_CHANNEL_VALUE_MID = 992;
static constexpr uint16_t CRSF_CHANNEL_VALUE_MAX = 1792;

// --------------------------------------------------
// Host packet definitions
// --------------------------------------------------
static constexpr uint8_t HOST_HEADER_1 = 0xAA;
static constexpr uint8_t HOST_HEADER_2 = 0x55;
static constexpr size_t HOST_PACKET_SIZE = 11;
static constexpr uint16_t UNUSED_BUTTON_CHANNEL_VALUE = CRSF_CHANNEL_VALUE_MIN;

// --------------------------------------------------
// Status packet to PC
// --------------------------------------------------
static constexpr uint8_t STATUS_HEADER_1 = 0x5A;
static constexpr uint8_t STATUS_HEADER_2 = 0xA5;
static constexpr uint8_t STATUS_PACKET_TYPE = 0x31;
static constexpr size_t STATUS_PACKET_SIZE = 20;

static constexpr int LOGICAL_BUTTON_REVERSE = 0;
static constexpr int LOGICAL_BUTTON_DRIVE = 1;
static constexpr int LOGICAL_BUTTON_CAMERA_REAR = 2;
static constexpr int LOGICAL_BUTTON_SPORT = 3;
static constexpr int LOGICAL_BUTTON_FLASH = 4;
static constexpr int LOGICAL_BUTTON_MAIN_LIGHT = 5;
static constexpr int LOGICAL_BUTTON_FRONT_DIFF_LOCKED = 6;
static constexpr int LOGICAL_BUTTON_REAR_DIFF_LOCKED = 7;
static constexpr int LOGICAL_BUTTON_COUNT = 8;
static constexpr uint16_t CONTROL_WORD_MASK = 0x07FF;
static constexpr int CAMERA_ANGLE_CODE_SHIFT = 8;
static constexpr uint8_t CAMERA_ANGLE_CENTER_CODE = 3;
static constexpr uint8_t CAMERA_ANGLE_MAX_CODE = 5;
static constexpr uint16_t DEFAULT_CONTROL_WORD = (uint16_t)(CAMERA_ANGLE_CENTER_CODE << CAMERA_ANGLE_CODE_SHIFT);
static constexpr int CAMERA_ANGLE_MIN_DEG = -90;
static constexpr int CAMERA_ANGLE_STEP_DEG = 45;
static constexpr uint8_t CONTROL_SEGMENT_COUNT = 4;
static constexpr uint8_t CONTROL_SEGMENT_PAYLOAD_MASK = 0x07;
static constexpr uint16_t CONTROL_SYMBOL_MIN_VALUE = 220;
static constexpr uint16_t CONTROL_SYMBOL_STEP_VALUE = 48;

// --------------------------------------------------
// Structs
// --------------------------------------------------
struct Steuerdaten {
    int lenkung_us;
    int gas_us;
    int bremse_us;
    uint16_t control_word;
    int camera_angle_deg;
    bool button[LOGICAL_BUTTON_COUNT];
};

struct FahrzeugStatus {
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
};

struct DebugStats {
    uint32_t host_packets_decoded;
    uint32_t telemetry_bytes_seen;
    uint32_t telemetry_frames_seen;
    uint32_t telemetry_crc_errors;
    uint32_t flight_mode_frames_seen;
    uint32_t battery_frames_seen;
    uint32_t link_stats_frames_seen;
    uint32_t device_info_frames_seen;
    uint8_t last_telemetry_type;
};

// --------------------------------------------------
// Globals
// --------------------------------------------------
static Steuerdaten s_state = {
    .lenkung_us = 1500,
    .gas_us = 1000,
    .bremse_us = 1000,
    .control_word = DEFAULT_CONTROL_WORD,
    .camera_angle_deg = 0,
};

static FahrzeugStatus s_vehicle_status = {
    .valid = false,
    .gear = 'N',
};

static DebugStats s_debug_stats = {};

static uint16_t s_tx_channels[CRSF_NUM_CHANNELS];
static uint8_t s_tx_frame[CRSF_TOTAL_FRAME_SIZE];

static uint8_t s_host_packet[HOST_PACKET_SIZE];
static uint8_t s_host_idx = 0;

static uint8_t s_rx_frame[CRSF_MAX_FRAME_SIZE];
static uint8_t s_rx_frame_idx = 0;
static uint8_t s_rx_expected_total = 0;

static bool s_crsf_tx_attached = false;
static uint8_t s_control_segment = 0;

// --------------------------------------------------
// CRC8 Lookup Table, Poly 0xD5
// --------------------------------------------------
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

static uint8_t crc8(const uint8_t *ptr, uint8_t len)
{
    uint8_t crc = 0;

    while (len--) {
        crc = crc8tab[crc ^ *ptr++];
    }

    return crc;
}

// --------------------------------------------------
// Helper
// --------------------------------------------------
static uint32_t millis_now()
{
    return millis();
}

static uint16_t clampCh(int v)
{
    if (v < (int)CRSF_CHANNEL_VALUE_MIN) return CRSF_CHANNEL_VALUE_MIN;
    if (v > (int)CRSF_CHANNEL_VALUE_MAX) return CRSF_CHANNEL_VALUE_MAX;
    return (uint16_t)v;
}

static int clampUs(int us)
{
    if (us < 1000) return 1000;
    if (us > 2000) return 2000;
    return us;
}

static uint16_t usToCRSF(int us)
{
    int v = ((us - 1500) * 8 / 5) + 992;
    return clampCh(v);
}

static int normSteerToUs(int16_t steer)
{
    return clampUs(1500 + ((int)steer / 2));
}

static int normPedalToUs(uint16_t pedal)
{
    return clampUs(1000 + (int)pedal);
}

static uint8_t cameraAngleCodeFromControlWord(uint16_t control_word)
{
    return (uint8_t)((control_word >> CAMERA_ANGLE_CODE_SHIFT) & 0x07U);
}

static int cameraAngleDegFromCode(uint8_t angle_code)
{
    if ((angle_code < 1U) || (angle_code > CAMERA_ANGLE_MAX_CODE)) {
        return 0;
    }

    return CAMERA_ANGLE_MIN_DEG + (((int)angle_code - 1) * CAMERA_ANGLE_STEP_DEG);
}

static uint16_t controlSymbolForSegment(uint16_t control_word, uint8_t segment)
{
    uint8_t payload = 0;

    switch (segment) {
        case 0:
            payload = (uint8_t)(control_word & 0x07U);
            break;
        case 1:
            payload = (uint8_t)((control_word >> 3U) & 0x07U);
            break;
        case 2:
            payload = (uint8_t)((control_word >> 6U) & 0x03U);
            break;
        case 3:
            payload = (uint8_t)((control_word >> CAMERA_ANGLE_CODE_SHIFT) & CONTROL_SEGMENT_PAYLOAD_MASK);
            break;
        default:
            payload = 0;
            break;
    }

    const uint8_t symbol = (uint8_t)((segment << 3U) | payload);

    return (uint16_t)(CONTROL_SYMBOL_MIN_VALUE + ((uint16_t)symbol * CONTROL_SYMBOL_STEP_VALUE));
}

static bool logicalButtonActive(int button_index)
{
    if ((button_index < 0) || (button_index >= LOGICAL_BUTTON_COUNT)) {
        return false;
    }

    return s_state.button[button_index];
}

static char commandedGear()
{
    const bool reverse_selected = logicalButtonActive(LOGICAL_BUTTON_REVERSE);
    const bool drive_selected = logicalButtonActive(LOGICAL_BUTTON_DRIVE);

    if (reverse_selected == drive_selected) {
        return 'N';
    }

    return reverse_selected ? 'R' : 'D';
}

static bool commandedSportMode()
{
    return logicalButtonActive(LOGICAL_BUTTON_SPORT);
}

static bool commandedMainLight()
{
    return logicalButtonActive(LOGICAL_BUTTON_MAIN_LIGHT);
}

static bool commandedCameraRear()
{
    return logicalButtonActive(LOGICAL_BUTTON_CAMERA_REAR);
}

static bool commandedFrontDiffLocked()
{
    return logicalButtonActive(LOGICAL_BUTTON_FRONT_DIFF_LOCKED);
}

static bool commandedRearDiffLocked()
{
    return logicalButtonActive(LOGICAL_BUTTON_REAR_DIFF_LOCKED);
}

static uint8_t commandedCameraAngleCode()
{
    uint8_t angle_code = cameraAngleCodeFromControlWord(s_state.control_word);
    return ((angle_code >= 1U) && (angle_code <= CAMERA_ANGLE_MAX_CODE)) ? angle_code : CAMERA_ANGLE_CENTER_CODE;
}

static int commandedCameraAngleDeg()
{
    return cameraAngleDegFromCode(commandedCameraAngleCode());
}

// --------------------------------------------------
// CRSF Send / Listen Mode
// --------------------------------------------------
static void enterCrsfListenMode()
{
    if (s_crsf_tx_attached) {
        pinMatrixOutDetach(CRSF_DATA_PIN, false, false);
        s_crsf_tx_attached = false;
    }

    gpio_set_direction((gpio_num_t)CRSF_DATA_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CRSF_DATA_PIN, GPIO_PULLUP_ONLY);
}

static void enterCrsfSendMode()
{
    gpio_set_direction((gpio_num_t)CRSF_DATA_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CRSF_DATA_PIN, GPIO_PULLUP_ONLY);

    ESP_ERROR_CHECK(uart_set_pin(
        UART_CRSF,
        CRSF_DATA_PIN,
        CRSF_DATA_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    s_crsf_tx_attached = true;
}

// --------------------------------------------------
// Channels
// --------------------------------------------------
static void fillChannels(uint16_t ch[CRSF_NUM_CHANNELS])
{
    for (int i = 0; i < CRSF_NUM_CHANNELS; ++i) {
        ch[i] = CRSF_CHANNEL_VALUE_MID;
    }

    ch[0] = usToCRSF(s_state.lenkung_us);
    ch[1] = usToCRSF(s_state.gas_us);
    ch[2] = usToCRSF(s_state.bremse_us);
    ch[3] = controlSymbolForSegment(s_state.control_word & CONTROL_WORD_MASK, s_control_segment);
    s_control_segment = (uint8_t)((s_control_segment + 1U) % CONTROL_SEGMENT_COUNT);

    for (int i = 4; i < CRSF_NUM_CHANNELS; ++i) {
        ch[i] = UNUSED_BUTTON_CHANNEL_VALUE;
    }
}

// --------------------------------------------------
// CRSF Frame Build
// --------------------------------------------------
static void buildCrsfRcChannelsFrame(uint8_t out[CRSF_TOTAL_FRAME_SIZE], const uint16_t chIn[CRSF_NUM_CHANNELS])
{
    out[0] = CRSF_SYNC_BYTE;
    out[1] = CRSF_LEN_FIELD;
    out[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memset(&out[3], 0, CRSF_PAYLOAD_SIZE);

    uint32_t bitbuf = 0;
    uint8_t bitcnt = 0;
    int p = 0;

    for (int i = 0; i < CRSF_NUM_CHANNELS; ++i) {
        const uint16_t v = clampCh(chIn[i]) & 0x07FF;

        bitbuf |= ((uint32_t)v) << bitcnt;
        bitcnt += 11;

        while (bitcnt >= 8) {
            if (p < CRSF_PAYLOAD_SIZE) {
                out[3 + p] = (uint8_t)(bitbuf & 0xFF);
            }

            ++p;
            bitbuf >>= 8;
            bitcnt -= 8;
        }
    }

    if (p < CRSF_PAYLOAD_SIZE && bitcnt > 0) {
        out[3 + p] = (uint8_t)(bitbuf & 0xFF);
    }

    out[CRSF_TOTAL_FRAME_SIZE - 1] = crc8(&out[2], 1 + CRSF_PAYLOAD_SIZE);
}

// --------------------------------------------------
// Telemetry Parser
// --------------------------------------------------
static bool isValidSyncByte(uint8_t value)
{
    switch (value) {
        case 0x00:
        case 0xC8:
        case 0xEA:
        case 0xEC:
        case 0xEE:
            return true;

        default:
            return false;
    }
}

static void resetTelemetryParser()
{
    s_rx_frame_idx = 0;
    s_rx_expected_total = 0;
}

static void parseFlightModePayload(const uint8_t *payload, size_t payload_length)
{
    char mode_text[32];
    char gear = 'N';
    char drive_mode[8] = {0};
    unsigned int light = 0;
    unsigned int camera = 0;
    int battery_temp_c = 0;

    size_t copy_len = payload_length;

    if (copy_len >= sizeof(mode_text)) {
        copy_len = sizeof(mode_text) - 1;
    }

    memcpy(mode_text, payload, copy_len);
    mode_text[copy_len] = '\0';

    if (sscanf(mode_text, "T%d", &battery_temp_c) == 1) {
        s_vehicle_status.valid = true;
        s_vehicle_status.battery_temp_c = (int16_t)battery_temp_c;
        s_vehicle_status.last_update_ms = millis_now();
        s_vehicle_status.last_link_activity_ms = millis_now();
        return;
    }

    int parsed = sscanf(
        mode_text,
        "%c|%7[^|]|L%u|C%u|T%d",
        &gear,
        drive_mode,
        &light,
        &camera,
        &battery_temp_c
    );

    // Rueckwaertskompatibel zum alten Format ohne Temperatur:
    // Beispiel alt: D|SPORT|L1|C0
    // Beispiel neu: D|SPORT|L1|C0|T35
    if (parsed < 4) {
        parsed = sscanf(
            mode_text,
            "%c|%7[^|]|L%u|C%u",
            &gear,
            drive_mode,
            &light,
            &camera
        );
    }

    if (parsed >= 4) {
        s_vehicle_status.valid = true;
        s_vehicle_status.gear = gear;
        s_vehicle_status.sport_mode = strcmp(drive_mode, "SPORT") == 0;
        s_vehicle_status.main_light_on = (light != 0);
        s_vehicle_status.camera_rear_active = (camera != 0);

        if (parsed == 5) {
            s_vehicle_status.battery_temp_c = (int16_t)battery_temp_c;
        }

        s_vehicle_status.last_update_ms = millis_now();
        s_vehicle_status.last_link_activity_ms = millis_now();
    }
}

static void parseBatteryPayload(const uint8_t *payload, size_t payload_length)
{
    if (payload_length < 8) {
        return;
    }

    s_vehicle_status.battery_mv = (uint16_t)((((uint16_t)payload[0] << 8) | payload[1]) * 10U);
    s_vehicle_status.battery_percent = payload[7];

    s_vehicle_status.last_update_ms = millis_now();
    s_vehicle_status.last_link_activity_ms = millis_now();
}

static void parseLinkStatisticsPayload(const uint8_t *payload, size_t payload_length)
{
    if (payload_length < 10) {
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

static void processTelemetryFrame(const uint8_t *frame)
{
    const uint8_t length = frame[1];
    const uint8_t type = frame[2];
    const uint8_t received_crc = frame[1 + length];
    const uint8_t calculated_crc = crc8(&frame[2], (uint8_t)(length - 1));
    const uint8_t *payload = &frame[3];
    const size_t payload_length = (size_t)(length - 2);

    s_debug_stats.telemetry_frames_seen++;
    s_debug_stats.last_telemetry_type = type;

    if (received_crc != calculated_crc) {
        s_debug_stats.telemetry_crc_errors++;
        return;
    }

    // Jede gueltige CRSF-Telemetrie zeigt: Rueckkanal lebt.
    s_vehicle_status.last_link_activity_ms = millis_now();

    if (type == CRSF_FRAMETYPE_FLIGHT_MODE) {
        s_debug_stats.flight_mode_frames_seen++;
        parseFlightModePayload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_BATTERY_SENSOR) {
        s_debug_stats.battery_frames_seen++;
        parseBatteryPayload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_LINK_STATISTICS) {
        s_debug_stats.link_stats_frames_seen++;
        parseLinkStatisticsPayload(payload, payload_length);
        return;
    }

    if (type == CRSF_FRAMETYPE_DEVICE_INFO) {
        s_debug_stats.device_info_frames_seen++;
        return;
    }
}

static void processTelemetryByte(uint8_t value)
{
    if (s_rx_frame_idx == 0) {
        if (!isValidSyncByte(value)) {
            return;
        }

        s_rx_frame[s_rx_frame_idx++] = value;
        return;
    }

    if (s_rx_frame_idx == 1) {
        if ((value < CRSF_MIN_LENGTH_FIELD) || (value > CRSF_MAX_LENGTH_FIELD)) {
            if (isValidSyncByte(value)) {
                s_rx_frame[0] = value;
                s_rx_frame_idx = 1;
                s_rx_expected_total = 0;
            } else {
                resetTelemetryParser();
            }

            return;
        }

        s_rx_frame[s_rx_frame_idx++] = value;
        s_rx_expected_total = (uint8_t)(value + 2);

        if (s_rx_expected_total > CRSF_MAX_FRAME_SIZE) {
            resetTelemetryParser();
        }

        return;
    }

    s_rx_frame[s_rx_frame_idx++] = value;

    if (s_rx_frame_idx == s_rx_expected_total) {
        processTelemetryFrame(s_rx_frame);
        resetTelemetryParser();
        return;
    }

    if (s_rx_frame_idx >= CRSF_MAX_FRAME_SIZE) {
        resetTelemetryParser();
    }
}

static void readCrsfTelemetry()
{
    uint8_t buffer[64];

    int bytes_read = uart_read_bytes(UART_CRSF, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) {
        return;
    }

    s_debug_stats.telemetry_bytes_seen += (uint32_t)bytes_read;

    for (int i = 0; i < bytes_read; ++i) {
        processTelemetryByte(buffer[i]);
    }
}

// --------------------------------------------------
// Status to Host
// --------------------------------------------------
static uint8_t gearToCode(char gear)
{
    switch (gear) {
        case 'N': return 1;
        case 'D': return 2;
        case 'R': return 3;
        default: return 0;
    }
}

static void sendVehicleStatusToHost()
{
    static uint32_t last_status_ms = 0;

    uint8_t packet[STATUS_PACKET_SIZE];
    uint8_t checksum = 0;
    uint8_t flags = 0;

    uint32_t now_ms = millis_now();

    bool link_active = (now_ms - s_vehicle_status.last_link_activity_ms) <= STATUS_VALID_MS;
    bool vehicle_status_valid = s_vehicle_status.valid &&
                                ((now_ms - s_vehicle_status.last_update_ms) <= STATUS_VALID_MS);
    if ((now_ms - last_status_ms) < STATUS_PERIOD_MS) {
        return;
    }

    last_status_ms = now_ms;

    if (link_active) flags |= 0x01;
    if (commandedSportMode()) flags |= 0x02;
    if (commandedMainLight()) flags |= 0x04;
    if (vehicle_status_valid) flags |= 0x08;
    if (commandedCameraRear()) flags |= 0x10;
    if (commandedFrontDiffLocked()) flags |= 0x20;
    if (commandedRearDiffLocked()) flags |= 0x40;

    packet[0] = STATUS_HEADER_1;
    packet[1] = STATUS_HEADER_2;
    packet[2] = STATUS_PACKET_TYPE;
    packet[3] = flags;
    packet[4] = gearToCode(commandedGear());
    packet[5] = s_vehicle_status.battery_percent;

    packet[6] = (uint8_t)(s_vehicle_status.battery_mv & 0xFF);
    packet[7] = (uint8_t)((s_vehicle_status.battery_mv >> 8) & 0xFF);

    packet[8] = (uint8_t)(s_vehicle_status.battery_temp_c & 0xFF);
    packet[9] = (uint8_t)((s_vehicle_status.battery_temp_c >> 8) & 0xFF);

    packet[10] = s_vehicle_status.uplink_lq;
    packet[11] = s_vehicle_status.uplink_rssi;
    packet[12] = (uint8_t)s_vehicle_status.uplink_snr;

    packet[13] = s_vehicle_status.downlink_lq;
    packet[14] = s_vehicle_status.downlink_rssi;
    packet[15] = (uint8_t)s_vehicle_status.downlink_snr;

    packet[16] = s_vehicle_status.rf_profile;
    packet[17] = s_vehicle_status.tx_power;
    packet[18] = (uint8_t)((s_vehicle_status.sequence++ & 0x0FU) | (commandedCameraAngleCode() << 4));

    for (size_t i = 0; i < STATUS_PACKET_SIZE - 1; ++i) {
        checksum ^= packet[i];
    }

    packet[19] = checksum;

    if (Serial.availableForWrite() >= (int)sizeof(packet)) {
        Serial.write(packet, sizeof(packet));
    }
}

// --------------------------------------------------
// Debug
// --------------------------------------------------
static void sendDebugLine()
{
    static uint32_t last_debug_ms = 0;

    char line[240];
    uint32_t now_ms = millis_now();

    if ((now_ms - last_debug_ms) < DEBUG_PERIOD_MS) {
        return;
    }

    last_debug_ms = now_ms;

    snprintf(
        line,
        sizeof(line),
        "!dbg host=%lu rx=%lu frm=%lu crc=%lu fm=%lu bat=%lu ls=%lu dev=%lu last=%02X valid=%u gear=%c mv=%u pct=%u tmp=%d ulq=%u urssi=%u usnr=%d dlq=%u drssi=%u dsnr=%d ctl=%u cam=%d df=%u dr=%u\n",
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
        commandedGear(),
        (unsigned int)s_vehicle_status.battery_mv,
        (unsigned int)s_vehicle_status.battery_percent,
        (int)s_vehicle_status.battery_temp_c,
        (unsigned int)s_vehicle_status.uplink_lq,
        (unsigned int)s_vehicle_status.uplink_rssi,
        (int)s_vehicle_status.uplink_snr,
        (unsigned int)s_vehicle_status.downlink_lq,
        (unsigned int)s_vehicle_status.downlink_rssi,
        (int)s_vehicle_status.downlink_snr,
        (unsigned int)(s_state.control_word & CONTROL_WORD_MASK),
        commandedCameraAngleDeg(),
        commandedFrontDiffLocked() ? 1U : 0U,
        commandedRearDiffLocked() ? 1U : 0U
    );

    if (Serial.availableForWrite() >= (int)strlen(line)) {
        Serial.write((const uint8_t *)line, strlen(line));
    }
}

// --------------------------------------------------
// Host Parser
// --------------------------------------------------
static void resetHostParser()
{
    s_host_idx = 0;
}

static bool decodeHostPacket(const uint8_t *packet, Steuerdaten &out_state)
{
    uint8_t checksum = 0;

    for (size_t i = 0; i < HOST_PACKET_SIZE - 1; ++i) {
        checksum ^= packet[i];
    }

    if (checksum != packet[HOST_PACKET_SIZE - 1]) {
        return false;
    }

    const int16_t steer = (int16_t)(packet[2] | (packet[3] << 8));
    const uint16_t throttle = (uint16_t)(packet[4] | (packet[5] << 8));
    const uint16_t brake = (uint16_t)(packet[6] | (packet[7] << 8));
    const uint16_t buttons = (uint16_t)(packet[8] | (packet[9] << 8));

    out_state.lenkung_us = normSteerToUs(steer);
    out_state.gas_us = normPedalToUs(throttle);
    out_state.bremse_us = normPedalToUs(brake);
    out_state.control_word = buttons & CONTROL_WORD_MASK;
    out_state.camera_angle_deg = cameraAngleDegFromCode(cameraAngleCodeFromControlWord(out_state.control_word));

    for (int i = 0; i < LOGICAL_BUTTON_COUNT; ++i) {
        out_state.button[i] = ((buttons >> i) & 0x01U) != 0;
    }

    s_debug_stats.host_packets_decoded++;
    return true;
}

static void readLaptopData()
{
    while (Serial.available() > 0) {
        uint8_t b = (uint8_t)Serial.read();

        if (s_host_idx == 0) {
            if (b == HOST_HEADER_1) {
                s_host_packet[s_host_idx++] = b;
            }

            continue;
        }

        if (s_host_idx == 1) {
            if (b == HOST_HEADER_2) {
                s_host_packet[s_host_idx++] = b;
            } else if (b == HOST_HEADER_1) {
                s_host_packet[0] = HOST_HEADER_1;
                s_host_idx = 1;
            } else {
                resetHostParser();
            }

            continue;
        }

        s_host_packet[s_host_idx++] = b;

        if (s_host_idx == HOST_PACKET_SIZE) {
            Steuerdaten next_state;

            if (decodeHostPacket(s_host_packet, next_state)) {
                s_state = next_state;
            }

            resetHostParser();
        }
    }
}

// --------------------------------------------------
// UART init
// --------------------------------------------------
static void init_uart_singlewire()
{
    uart_config_t cfg = {};
    cfg.baud_rate = (int)CRSF_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_CRSF, 256, 1024, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CRSF, &cfg));

    // Wichtig:
    // Kein UART_MODE_RS485_HALF_DUPLEX.
    // Die Single-Wire-Umschaltung machen wir manuell.
    ESP_ERROR_CHECK(uart_set_mode(UART_CRSF, UART_MODE_UART));
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_CRSF, UART_SIGNAL_INV_DISABLE));

    enterCrsfSendMode();
    enterCrsfListenMode();
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup()
{
    Serial.begin(USB_BAUD);
    Serial.setRxBufferSize(256);
    Serial.setTxBufferSize(2048);

    for (int i = 0; i < 13; ++i) {
        s_state.button[i] = false;
    }

    s_vehicle_status.valid = false;
    s_vehicle_status.gear = 'N';
    s_vehicle_status.sport_mode = false;
    s_vehicle_status.main_light_on = false;
    s_vehicle_status.camera_rear_active = false;
    s_vehicle_status.battery_mv = 0;
    s_vehicle_status.battery_percent = 0;
    s_vehicle_status.battery_temp_c = 0;
    s_vehicle_status.uplink_lq = 0;
    s_vehicle_status.uplink_rssi = 0;
    s_vehicle_status.uplink_snr = 0;
    s_vehicle_status.downlink_lq = 0;
    s_vehicle_status.downlink_rssi = 0;
    s_vehicle_status.downlink_snr = 0;
    s_vehicle_status.rf_profile = 0;
    s_vehicle_status.tx_power = 0;
    s_vehicle_status.last_update_ms = 0;
    s_vehicle_status.last_link_activity_ms = 0;
    s_vehicle_status.sequence = 0;

    memset(&s_debug_stats, 0, sizeof(s_debug_stats));

    resetHostParser();
    resetTelemetryParser();

    init_uart_singlewire();
}

// --------------------------------------------------
// Loop
// --------------------------------------------------
void loop()
{
    readLaptopData();
    readCrsfTelemetry();
    sendVehicleStatusToHost();
    sendDebugLine();

    static uint32_t next_send_us = 0;
    const uint32_t now_us = micros();

    if (next_send_us == 0) {
        next_send_us = now_us;
    }

    if ((int32_t)(now_us - next_send_us) >= 0) {
        next_send_us += SEND_PERIOD_US;

        fillChannels(s_tx_channels);
        buildCrsfRcChannelsFrame(s_tx_frame, s_tx_channels);

        enterCrsfSendMode();
        uart_write_bytes(UART_CRSF, (const char *)s_tx_frame, sizeof(s_tx_frame));
        uart_wait_tx_done(UART_CRSF, pdMS_TO_TICKS(2));
        enterCrsfListenMode();
    }
}
