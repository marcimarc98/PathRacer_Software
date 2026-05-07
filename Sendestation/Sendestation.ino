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

static constexpr uart_port_t UART_DEBUG = UART_NUM_2;
static constexpr int DEBUG_TX_PIN = 16;
static constexpr uint32_t DEBUG_BAUD = 115200;
static constexpr uint32_t DEBUG_PERIOD_MS = 100;

static constexpr uint32_t USB_BAUD       = 460800;
static constexpr uint32_t CRSF_BAUD      = 420000;
static constexpr uint32_t SEND_PERIOD_US = 4000;   // 250 Hz
static constexpr uint32_t STATUS_PERIOD_MS = 100;
static constexpr uint32_t STATUS_VALID_MS = 3000;

static constexpr bool INVERT_TX = false;

// --------------------------------------------------
// CRSF DEFINITIONS
// --------------------------------------------------
#define CRSF_SYNC_BYTE                    0xC8
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED 0x16
#define CRSF_FRAMETYPE_BATTERY_SENSOR     0x08
#define CRSF_FRAMETYPE_FLIGHT_MODE        0x21
#define CRSF_FRAMETYPE_DEVICE_PING        0x28
#define CRSF_FRAMETYPE_DEVICE_INFO        0x29
#define CRSF_MAX_FRAME_SIZE               64
#define CRSF_MIN_LENGTH_FIELD             2
#define CRSF_MAX_LENGTH_FIELD             62
#define CRSF_ADDRESS_RADIO                0xEA
#define CRSF_ADDRESS_TX_MODULE            0xEE

#define CRSF_NUM_CHANNELS      16
#define CRSF_PAYLOAD_SIZE      22
#define CRSF_LEN_FIELD         (1 + CRSF_PAYLOAD_SIZE + 1)
#define CRSF_TOTAL_FRAME_SIZE  (2 + CRSF_LEN_FIELD)

#define CRSF_CHANNEL_VALUE_MIN 192
#define CRSF_CHANNEL_VALUE_MID 992
#define CRSF_CHANNEL_VALUE_MAX 1792

// --------------------------------------------------
// Host packet definitions
// --------------------------------------------------
static constexpr uint8_t HOST_HEADER_1 = 0xAA;
static constexpr uint8_t HOST_HEADER_2 = 0x55;
static constexpr size_t HOST_PACKET_SIZE = 11;
static constexpr int UNUSED_BUTTON_CHANNEL_VALUE = CRSF_CHANNEL_VALUE_MIN;

static constexpr uint8_t STATUS_HEADER_1 = 0x5A;
static constexpr uint8_t STATUS_HEADER_2 = 0xA5;
static constexpr uint8_t STATUS_PACKET_TYPE = 0x31;
static constexpr size_t STATUS_PACKET_SIZE = 10;

// --------------------------------------------------
// Struct
// --------------------------------------------------
typedef struct {
    int Lenkung_us;
    int Gas_us;
    int Bremse_us;
    bool Knopf[13];
} Steuerdaten;

typedef struct {
    bool valid;
    char gear;
    bool sportMode;
    bool mainLightOn;
    bool cameraRearActive;
    uint16_t batteryMv;
    uint8_t batteryPercent;
    uint32_t lastUpdateMs;
    uint32_t lastLinkActivityMs;
    uint8_t sequence;
} FahrzeugStatus;

typedef struct {
    uint32_t hostPacketsDecoded;
    uint32_t telemetryBytesSeen;
    uint32_t telemetryFramesSeen;
    uint32_t telemetryCrcErrors;
    uint32_t flightModeFramesSeen;
    uint32_t batteryFramesSeen;
    uint32_t deviceInfoFramesSeen;
    uint8_t lastTelemetryType;
    uint8_t rawSample[16];
    uint8_t rawSampleCount;
} DebugStats;

static Steuerdaten s;
static FahrzeugStatus s_vehicleStatus;
static DebugStats s_debugStats;
static uint16_t txChannels[CRSF_NUM_CHANNELS];
static uint8_t txFrame[CRSF_TOTAL_FRAME_SIZE];
static uint8_t hostPacket[HOST_PACKET_SIZE];
static uint8_t hostIdx = 0;
static uint16_t s_lastButtonsMask = 0;
static uint8_t rxFrame[CRSF_MAX_FRAME_SIZE];
static uint8_t rxFrameIdx = 0;
static uint8_t rxExpectedTotal = 0;
static bool s_crsfTxAttached = false;

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

static uint8_t crc8(const uint8_t *ptr, uint8_t len) {
    uint8_t crc = 0;

    while (len--) {
        crc = crc8tab[crc ^ *ptr++];
    }

    return crc;
}

// --------------------------------------------------
// Hilfsfunktionen
// --------------------------------------------------
static inline uint16_t clampCh(int v) {
    if (v < CRSF_CHANNEL_VALUE_MIN) return CRSF_CHANNEL_VALUE_MIN;
    if (v > CRSF_CHANNEL_VALUE_MAX) return CRSF_CHANNEL_VALUE_MAX;
    return (uint16_t)v;
}

static inline int clampUs(int us) {
    if (us < 1000) return 1000;
    if (us > 2000) return 2000;
    return us;
}

static inline uint16_t usToCRSF(int us) {
    int v = ((us - 1500) * 8 / 5) + 992;
    return clampCh(v);
}

static inline uint16_t boolToCRSF(bool b) {
    return b ? CRSF_CHANNEL_VALUE_MAX : CRSF_CHANNEL_VALUE_MIN;
}

static inline uint16_t channelValueForButton(const Steuerdaten &state, int buttonIndex) {
    return ((buttonIndex >= 0) && (buttonIndex < 13)) ? boolToCRSF(state.Knopf[buttonIndex]) : CRSF_CHANNEL_VALUE_MIN;
}

static uint16_t readButtonsMask(const Steuerdaten &state) {
    uint16_t mask = 0;

    for (int i = 0; i < 13; i++) {
        if (state.Knopf[i]) {
            mask |= (uint16_t)(1U << i);
        }
    }

    return mask;
}

static inline int normSteerToUs(int16_t steer) {
    return clampUs(1500 + ((int)steer / 2));
}

static inline int normPedalToUs(uint16_t pedal) {
    return clampUs(1000 + (int)pedal);
}

// --------------------------------------------------
// Struct -> Kanaele
// --------------------------------------------------
static void fillChannels(uint16_t ch[CRSF_NUM_CHANNELS]) {
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        ch[i] = CRSF_CHANNEL_VALUE_MID;
    }

    ch[0] = usToCRSF(s.Lenkung_us);
    ch[1] = usToCRSF(s.Gas_us);
    ch[2] = usToCRSF(s.Bremse_us);

    // Low ELRS channels only, compact receiver button numbering:
    // CH4  -> Button 0 (Down Shift, host button 0)
    // CH5  -> Button 1 (Up Shift,   host button 1)
    // CH6  -> Button 2 (R2,         host button 8)
    // CH7  -> Button 3 (L2,         host button 9)
    // CH8  -> Button 4 (L1,         host button 10)
    // CH9  -> Button 5 (R1,         host button 11)
    // CH10 -> Button 6 (PS,         host button 12)
    ch[3] = channelValueForButton(s, 0);
    ch[4] = channelValueForButton(s, 1);
    ch[5] = channelValueForButton(s, 8);
    ch[6] = channelValueForButton(s, 9);
    ch[7] = channelValueForButton(s, 10);
    ch[8] = channelValueForButton(s, 11);
    ch[9] = channelValueForButton(s, 12);

    for (int i = 10; i < CRSF_NUM_CHANNELS; i++) {
        ch[i] = UNUSED_BUTTON_CHANNEL_VALUE;
    }
}

// --------------------------------------------------
// Framebau
// --------------------------------------------------
static void buildCrsfRcChannelsFrame(uint8_t out[CRSF_TOTAL_FRAME_SIZE], const uint16_t chIn[CRSF_NUM_CHANNELS]) {
    out[0] = CRSF_SYNC_BYTE;
    out[1] = (uint8_t)CRSF_LEN_FIELD;
    out[2] = (uint8_t)CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    for (int i = 0; i < CRSF_PAYLOAD_SIZE; i++) {
        out[3 + i] = 0;
    }

    uint32_t bitbuf = 0;
    uint8_t bitcnt = 0;
    int p = 0;

    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        const uint16_t v = clampCh(chIn[i]) & 0x07FF;

        bitbuf |= ((uint32_t)v) << bitcnt;
        bitcnt += 11;

        while (bitcnt >= 8) {
            if (p < CRSF_PAYLOAD_SIZE) {
                out[3 + p] = (uint8_t)(bitbuf & 0xFF);
            }

            p++;
            bitbuf >>= 8;
            bitcnt -= 8;
        }
    }

    if (p < CRSF_PAYLOAD_SIZE && bitcnt > 0) {
        out[3 + p] = (uint8_t)(bitbuf & 0xFF);
    }

    out[CRSF_TOTAL_FRAME_SIZE - 1] = crc8(&out[2], 1 + CRSF_PAYLOAD_SIZE);
}

static uint16_t unpack11(const uint8_t *payload, uint32_t channel) {
    const uint32_t bit_index = channel * 11U;
    const uint32_t byte_index = bit_index >> 3;
    const uint32_t shift = bit_index & 7U;
    uint32_t word = (uint32_t)payload[byte_index];

    if ((byte_index + 1U) < CRSF_PAYLOAD_SIZE) {
        word |= ((uint32_t)payload[byte_index + 1U]) << 8U;
    }

    if ((byte_index + 2U) < CRSF_PAYLOAD_SIZE) {
        word |= ((uint32_t)payload[byte_index + 2U]) << 16U;
    }

    return (uint16_t)((word >> shift) & 0x07FFU);
}

static bool frameMatchesChannels(const uint8_t frame[CRSF_TOTAL_FRAME_SIZE], const uint16_t channels[CRSF_NUM_CHANNELS]) {
    const uint8_t *payload = &frame[3];

    for (uint32_t i = 0; i < CRSF_NUM_CHANNELS; i++) {
        if (unpack11(payload, i) != (clampCh((int)channels[i]) & 0x07FFU)) {
            return false;
        }
    }

    return true;
}

static bool isValidSyncByte(uint8_t value) {
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

static void enterCrsfListenMode() {
    if (s_crsfTxAttached) {
        pinMatrixOutDetach(CRSF_DATA_PIN, false, false);
        s_crsfTxAttached = false;
    }

    gpio_set_direction((gpio_num_t)CRSF_DATA_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CRSF_DATA_PIN, GPIO_PULLUP_ONLY);
}

static void enterCrsfSendMode() {
    gpio_set_direction((gpio_num_t)CRSF_DATA_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CRSF_DATA_PIN, GPIO_PULLUP_ONLY);
    ESP_ERROR_CHECK(uart_set_pin(UART_CRSF, CRSF_DATA_PIN, CRSF_DATA_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    s_crsfTxAttached = true;
}

static inline void resetTelemetryParser() {
    rxFrameIdx = 0;
    rxExpectedTotal = 0;
}

static uint8_t gearToCode(char gear) {
    switch (gear) {
        case 'N': return 1U;
        case 'D': return 2U;
        case 'R': return 3U;
        default: return 0U;
    }
}

static void parseFlightModePayload(const uint8_t* payload, size_t payloadLength) {
    char modeText[32];
    char gear = 'N';
    char driveMode[8] = {0};
    unsigned int light = 0;
    unsigned int camera = 0;
    const size_t copyLength = min(payloadLength, sizeof(modeText) - 1U);

    memcpy(modeText, payload, copyLength);
    modeText[copyLength] = '\0';

    if (sscanf(modeText, "%c|%7[^|]|L%u|C%u", &gear, driveMode, &light, &camera) == 4) {
        s_vehicleStatus.valid = true;
        s_vehicleStatus.gear = gear;
        s_vehicleStatus.sportMode = strcmp(driveMode, "SPORT") == 0;
        s_vehicleStatus.mainLightOn = light != 0U;
        s_vehicleStatus.cameraRearActive = camera != 0U;
        s_vehicleStatus.lastUpdateMs = millis();
    }
}

static void parseBatteryPayload(const uint8_t* payload, size_t payloadLength) {
    if (payloadLength < 8U) {
        return;
    }

    s_vehicleStatus.batteryMv = (uint16_t)(((payload[0] << 8) | payload[1]) * 10U);
    s_vehicleStatus.batteryPercent = payload[7];
    s_vehicleStatus.lastUpdateMs = millis();
}

static void processTelemetryFrame(const uint8_t* frame) {
    const uint8_t length = frame[1];
    const uint8_t type = frame[2];
    const uint8_t receivedCrc = frame[1U + length];
    const uint8_t calculatedCrc = crc8(&frame[2], (uint8_t)(length - 1U));
    const uint8_t* payload = &frame[3];
    const size_t payloadLength = (size_t)(length - 2U);

    s_debugStats.telemetryFramesSeen++;
    s_debugStats.lastTelemetryType = type;

    if (receivedCrc != calculatedCrc) {
        s_debugStats.telemetryCrcErrors++;
        return;
    }

    s_vehicleStatus.lastLinkActivityMs = millis();

    if (type == CRSF_FRAMETYPE_FLIGHT_MODE) {
        s_debugStats.flightModeFramesSeen++;
        parseFlightModePayload(payload, payloadLength);
        return;
    }

    if (type == CRSF_FRAMETYPE_BATTERY_SENSOR) {
        s_debugStats.batteryFramesSeen++;
        parseBatteryPayload(payload, payloadLength);
        return;
    }

    if (type == CRSF_FRAMETYPE_DEVICE_INFO) {
        s_debugStats.deviceInfoFramesSeen++;
    }
}

static void processTelemetryByte(uint8_t value) {
    if (rxFrameIdx == 0U) {
        if (!isValidSyncByte(value)) {
            return;
        }

        rxFrame[rxFrameIdx++] = value;
        return;
    }

    if (rxFrameIdx == 1U) {
        if ((value < CRSF_MIN_LENGTH_FIELD) || (value > CRSF_MAX_LENGTH_FIELD)) {
            if (isValidSyncByte(value)) {
                rxFrame[0] = value;
                rxFrameIdx = 1U;
                rxExpectedTotal = 0U;
            } else {
                resetTelemetryParser();
            }
            return;
        }

        rxFrame[rxFrameIdx++] = value;
        rxExpectedTotal = (uint8_t)(value + 2U);

        if (rxExpectedTotal > CRSF_MAX_FRAME_SIZE) {
            resetTelemetryParser();
        }
        return;
    }

    rxFrame[rxFrameIdx++] = value;

    if (rxFrameIdx == rxExpectedTotal) {
        processTelemetryFrame(rxFrame);
        resetTelemetryParser();
        return;
    }

    if (rxFrameIdx >= CRSF_MAX_FRAME_SIZE) {
        resetTelemetryParser();
    }
}

// --------------------------------------------------
// CRSF UART
// --------------------------------------------------
static void init_uart_crsf_singlewire() {
    uart_config_t cfg = {};
    cfg.baud_rate = (int)CRSF_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity    = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_CRSF, 256, 1024, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CRSF, &cfg));
    ESP_ERROR_CHECK(uart_set_mode(UART_CRSF, UART_MODE_UART));

    if (INVERT_TX) {
        ESP_ERROR_CHECK(uart_set_line_inverse(UART_CRSF, UART_SIGNAL_TXD_INV));
    } else {
        ESP_ERROR_CHECK(uart_set_line_inverse(UART_CRSF, UART_SIGNAL_INV_DISABLE));
    }

    enterCrsfSendMode();
    enterCrsfListenMode();
}

// --------------------------------------------------
// DEBUG UART
// --------------------------------------------------
static void init_uart_debug() {
    uart_config_t cfg = {};
    cfg.baud_rate = (int)DEBUG_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity    = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_DEBUG, 256, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_DEBUG, &cfg));

    ESP_ERROR_CHECK(uart_set_pin(
        UART_DEBUG,
        DEBUG_TX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
}

static void debugWriteLine(const char *text) {
    if (text == nullptr) {
        return;
    }

    uart_write_bytes(UART_DEBUG, text, strlen(text));
}

static void debugPrintState(uint16_t buttonsMask) {
    static uint32_t lastDebugMs = 0;
    char line[512];
    int offset = 0;
    const uint32_t nowMs = millis();
    const bool selfOk = frameMatchesChannels(txFrame, txChannels);

    if ((nowMs - lastDebugMs) < DEBUG_PERIOD_MS) {
        return;
    }

    lastDebugMs = nowMs;

    offset += snprintf(
        &line[offset],
        sizeof(line) - (size_t)offset,
        "btn %04X L %d G %d B %d self %u",
        (unsigned int)buttonsMask,
        s.Lenkung_us,
        s.Gas_us,
        s.Bremse_us,
        selfOk ? 1U : 0U);

    for (int i = 0; i < 13 && offset > 0 && (size_t)offset < sizeof(line); i++) {
        offset += snprintf(
            &line[offset],
            sizeof(line) - (size_t)offset,
            " %d %u",
            i,
            (unsigned int)txChannels[3 + i]);
    }

    offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, " P");

    for (int i = 0; i < CRSF_PAYLOAD_SIZE && offset > 0 && (size_t)offset < sizeof(line); i++) {
        offset += snprintf(
            &line[offset],
            sizeof(line) - (size_t)offset,
            " %02X",
            (unsigned int)txFrame[3 + i]);
    }

    if (offset > 0 && (size_t)offset < sizeof(line)) {
        offset += snprintf(&line[offset], sizeof(line) - (size_t)offset, "\r\n");
    }

    if (offset <= 0) {
        return;
    }

    line[sizeof(line) - 1] = '\0';
    debugWriteLine(line);
}

static void readCrsfTelemetry() {
    uint8_t buffer[64];
    const int bytesRead = uart_read_bytes(UART_CRSF, buffer, sizeof(buffer), 0);

    if (bytesRead <= 0) {
        return;
    }

    s_debugStats.telemetryBytesSeen += (uint32_t)bytesRead;

    for (int i = 0; i < bytesRead; i++) {
        if (s_debugStats.rawSampleCount < sizeof(s_debugStats.rawSample)) {
            s_debugStats.rawSample[s_debugStats.rawSampleCount++] = buffer[i];
        }
        processTelemetryByte(buffer[i]);
    }
}

static void sendUsbDebugLine() {
    static uint32_t lastDebugUsbMs = 0;
    char line[160];
    char rawHex[16 * 3 + 1];
    size_t rawOffset = 0;
    const uint32_t nowMs = millis();

    if ((nowMs - lastDebugUsbMs) < 1000U) {
        return;
    }

    lastDebugUsbMs = nowMs;

    rawHex[0] = '\0';
    for (uint8_t i = 0; i < s_debugStats.rawSampleCount && rawOffset + 4U < sizeof(rawHex); i++) {
        rawOffset += (size_t)snprintf(&rawHex[rawOffset], sizeof(rawHex) - rawOffset, "%02X", s_debugStats.rawSample[i]);
        if ((i + 1U) < s_debugStats.rawSampleCount && rawOffset + 2U < sizeof(rawHex)) {
            rawHex[rawOffset++] = '.';
            rawHex[rawOffset] = '\0';
        }
    }

    snprintf(
        line,
        sizeof(line),
        "!dbg host=%lu rx=%lu frm=%lu crc=%lu fm=%lu bat=%lu dev=%lu last=%02X valid=%u gear=%c mv=%u pct=%u raw=%s\n",
        (unsigned long)s_debugStats.hostPacketsDecoded,
        (unsigned long)s_debugStats.telemetryBytesSeen,
        (unsigned long)s_debugStats.telemetryFramesSeen,
        (unsigned long)s_debugStats.telemetryCrcErrors,
        (unsigned long)s_debugStats.flightModeFramesSeen,
        (unsigned long)s_debugStats.batteryFramesSeen,
        (unsigned long)s_debugStats.deviceInfoFramesSeen,
        (unsigned int)s_debugStats.lastTelemetryType,
        s_vehicleStatus.valid ? 1U : 0U,
        s_vehicleStatus.gear,
        (unsigned int)s_vehicleStatus.batteryMv,
        (unsigned int)s_vehicleStatus.batteryPercent,
        rawHex);
    Serial.print(line);
    s_debugStats.rawSampleCount = 0;
}

static void sendVehicleStatusToHost() {
    static uint32_t lastStatusMs = 0;
    uint8_t packet[STATUS_PACKET_SIZE];
    uint8_t checksum = 0;
    uint8_t flags = 0;
    const uint32_t nowMs = millis();
    const bool linkActive = (nowMs - s_vehicleStatus.lastLinkActivityMs) <= STATUS_VALID_MS;

    if ((nowMs - lastStatusMs) < STATUS_PERIOD_MS) {
        return;
    }

    lastStatusMs = nowMs;

    if (linkActive) flags |= 0x01U;
    if (s_vehicleStatus.sportMode) flags |= 0x02U;
    if (s_vehicleStatus.mainLightOn) flags |= 0x04U;
    if (s_vehicleStatus.cameraRearActive) flags |= 0x10U;

    packet[0] = STATUS_HEADER_1;
    packet[1] = STATUS_HEADER_2;
    packet[2] = STATUS_PACKET_TYPE;
    packet[3] = flags;
    packet[4] = gearToCode(s_vehicleStatus.gear);
    packet[5] = s_vehicleStatus.batteryPercent;
    packet[6] = (uint8_t)(s_vehicleStatus.batteryMv & 0xFFU);
    packet[7] = (uint8_t)((s_vehicleStatus.batteryMv >> 8U) & 0xFFU);
    packet[8] = s_vehicleStatus.sequence++;

    for (size_t i = 0; i < STATUS_PACKET_SIZE - 1U; i++) {
        checksum ^= packet[i];
    }

    packet[9] = checksum;
    Serial.write(packet, sizeof(packet));
}

// --------------------------------------------------
// Binaerparser fuer das Hostpaket
// --------------------------------------------------
static inline void resetHostParser() {
    hostIdx = 0;
}

static bool decodeHostPacket(const uint8_t *packet, Steuerdaten &out) {
    uint8_t checksum = 0;

    for (size_t i = 0; i < HOST_PACKET_SIZE - 1; i++) {
        checksum ^= packet[i];
    }

    if (checksum != packet[HOST_PACKET_SIZE - 1]) {
        return false;
    }

    const int16_t steer = (int16_t)(packet[2] | (packet[3] << 8));
    const uint16_t throttle = (uint16_t)(packet[4] | (packet[5] << 8));
    const uint16_t brake = (uint16_t)(packet[6] | (packet[7] << 8));
    const uint16_t buttons = (uint16_t)(packet[8] | (packet[9] << 8));

    out.Lenkung_us = normSteerToUs(steer);
    out.Gas_us = normPedalToUs(throttle);
    out.Bremse_us = normPedalToUs(brake);

    for (int i = 0; i < 13; i++) {
        out.Knopf[i] = ((buttons >> i) & 0x01U) != 0;
    }

    s_lastButtonsMask = buttons;
    s_debugStats.hostPacketsDecoded++;
    return true;
}

static void readLaptopData() {
    while (Serial.available() > 0) {
        uint8_t b = (uint8_t)Serial.read();

        if (hostIdx == 0) {
            if (b == HOST_HEADER_1) {
                hostPacket[hostIdx++] = b;
            }
            continue;
        }

        if (hostIdx == 1) {
            if (b == HOST_HEADER_2) {
                hostPacket[hostIdx++] = b;
            } else if (b == HOST_HEADER_1) {
                hostPacket[0] = HOST_HEADER_1;
                hostIdx = 1;
            } else {
                resetHostParser();
            }
            continue;
        }

        hostPacket[hostIdx++] = b;

        if (hostIdx == HOST_PACKET_SIZE) {
            Steuerdaten neu;

            if (decodeHostPacket(hostPacket, neu)) {
                s = neu;
            }

            resetHostParser();
        }
    }
}

void setup() {
    Serial.begin(USB_BAUD);
    Serial.setRxBufferSize(128);

    s.Lenkung_us = 1500;
    s.Gas_us     = 1500;
    s.Bremse_us  = 1000;

    for (int i = 0; i < 13; i++) {
        s.Knopf[i] = false;
    }

    s_vehicleStatus.valid = false;
    s_vehicleStatus.gear = 'N';
    s_vehicleStatus.sportMode = false;
    s_vehicleStatus.mainLightOn = false;
    s_vehicleStatus.cameraRearActive = false;
    s_vehicleStatus.batteryMv = 0;
    s_vehicleStatus.batteryPercent = 0;
    s_vehicleStatus.lastUpdateMs = 0;
    s_vehicleStatus.lastLinkActivityMs = 0;
    s_vehicleStatus.sequence = 0;
    memset(&s_debugStats, 0, sizeof(s_debugStats));

    resetTelemetryParser();
    init_uart_crsf_singlewire();
    init_uart_debug();
    debugWriteLine("ESP Debug UART aktiv\r\n");
}

void loop() {
    readLaptopData();
    readCrsfTelemetry();
    sendVehicleStatusToHost();
    sendUsbDebugLine();

    static uint32_t nextSendUs = 0;
    uint32_t nowUs = micros();

    if (nextSendUs == 0) {
        nextSendUs = nowUs;
    }

    if ((int32_t)(nowUs - nextSendUs) >= 0) {
        nextSendUs += SEND_PERIOD_US;

        fillChannels(txChannels);
        buildCrsfRcChannelsFrame(txFrame, txChannels);

        enterCrsfSendMode();
        uart_write_bytes(UART_CRSF, (const char*)txFrame, sizeof(txFrame));
        uart_wait_tx_done(UART_CRSF, pdMS_TO_TICKS(2));
        enterCrsfListenMode();
        debugPrintState(s_lastButtonsMask);
    }
}
