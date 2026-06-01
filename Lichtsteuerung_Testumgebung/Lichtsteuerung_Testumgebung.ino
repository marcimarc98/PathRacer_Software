/*
  Lichtsteuerung_Testumgebung
  Arduino Nano Testprogramm fuer eine Steuerplatine mit zwei MOSFET-Kanaelen.

  Verdrahtung:
  - Taster Frontlicht: D2 gegen GND, interner Pullup aktiv
  - Taster Bremslicht: D3 gegen GND, interner Pullup aktiv
  - MOSFET Frontscheinwerfer: D9  (PWM)
  - MOSFET Ruecklicht:          D10 (PWM)

  Getesteter Stand:
  - Frontscheinwerfer inkl. Tagfahrlicht, 100 % und Warn-Strobe funktioniert.
  - Ruecklicht/Bremslicht inkl. Mindestleuchtdauer und adaptivem Blinken funktioniert.
  - Ruecklicht-Hardware: 4 LEDs in Reihe mit 200-Ohm-Vorwiderstand.

  Annahme:
  - MOSFET ist aktiv, wenn der Arduino-Pin HIGH/PWM ausgibt.
  - Taster sind gedrueckt, wenn der Eingang LOW liest.
*/

#include <Arduino.h>

const byte PIN_BUTTON_FRONT = 2;
const byte PIN_BUTTON_BRAKE = 3;
const byte PIN_PWM_FRONT = 9;
const byte PIN_PWM_REAR = 10;

// PWM-Werte: 0 = aus, 255 = 100 %
const byte PWM_FRONT_OFF = 0;
const byte PWM_FRONT_DAYLIGHT = 15;  // ca. 35 %, bei Bedarf anpassen
const byte PWM_FRONT_FULL = 255;

const byte PWM_REAR_NORMAL = 65;     // ca. 25 %, Ruecklicht dauerhaft an
const byte PWM_REAR_BRAKE = 255;     // 100 %, Bremslicht

const unsigned long DEBOUNCE_MS = 35;
const unsigned long BRAKE_LIGHT_MIN_ON_MS = 1000;
const unsigned long ADAPTIVE_BRAKE_HOLD_MS = 1000;
const unsigned long ADAPTIVE_BRAKE_BLINK_MS = 150;
const unsigned long FRONT_STROBE_HOLD_MS = 1000;
const unsigned long FRONT_STROBE_BLINK_MS = 20;

enum FrontLightMode {
  FRONT_OFF,
  FRONT_DAYLIGHT,
  FRONT_FULL
};

struct DebouncedButton {
  byte pin;
  bool stablePressed;
  bool lastReadingPressed;
  bool pressedEvent;
  unsigned long lastReadingChangeMs;

  void begin(byte buttonPin) {
    pin = buttonPin;
    pinMode(pin, INPUT_PULLUP);

    stablePressed = isPressedRaw();
    lastReadingPressed = stablePressed;
    pressedEvent = false;
    lastReadingChangeMs = millis();
  }

  void update() {
    const bool readingPressed = isPressedRaw();
    pressedEvent = false;

    if (readingPressed != lastReadingPressed) {
      lastReadingPressed = readingPressed;
      lastReadingChangeMs = millis();
    }

    if ((millis() - lastReadingChangeMs) >= DEBOUNCE_MS &&
        readingPressed != stablePressed) {
      stablePressed = readingPressed;

      if (stablePressed) {
        pressedEvent = true;
      }
    }
  }

  bool wasPressed() const {
    return pressedEvent;
  }

  bool isPressed() const {
    return stablePressed;
  }

private:
  bool isPressedRaw() const {
    return digitalRead(pin) == LOW;
  }
};

DebouncedButton frontButton;
DebouncedButton brakeButton;
FrontLightMode frontLightMode = FRONT_OFF;
bool frontButtonWasPressed = false;
unsigned long frontButtonPressedSinceMs = 0;
bool brakeLightActive = false;
unsigned long brakeLightLastPressedMs = 0;
bool brakeButtonWasPressed = false;
unsigned long brakeButtonPressedSinceMs = 0;

void nextFrontLightMode();
void updateFrontLightButton();
void updateBrakeLightHold();
bool isBrakeLightOn();
bool isAdaptiveBrakeBlinking();
bool isFrontStrobeBlinking();
void applyOutputs();
byte frontPwmValue();
byte rearPwmValue();

void setup() {
  frontButton.begin(PIN_BUTTON_FRONT);
  brakeButton.begin(PIN_BUTTON_BRAKE);

  pinMode(PIN_PWM_FRONT, OUTPUT);
  pinMode(PIN_PWM_REAR, OUTPUT);

  applyOutputs();
}

void loop() {
  frontButton.update();
  brakeButton.update();

  updateFrontLightButton();
  updateBrakeLightHold();
  applyOutputs();
}

void nextFrontLightMode() {
  switch (frontLightMode) {
    case FRONT_OFF:
      frontLightMode = FRONT_DAYLIGHT;
      break;

    case FRONT_DAYLIGHT:
      frontLightMode = FRONT_FULL;
      break;

    case FRONT_FULL:
    default:
      frontLightMode = FRONT_OFF;
      break;
  }
}

void updateFrontLightButton() {
  const bool frontPressed = frontButton.isPressed();
  const unsigned long now = millis();

  if (frontPressed && !frontButtonWasPressed) {
    frontButtonPressedSinceMs = now;
  }

  if (!frontPressed && frontButtonWasPressed) {
    const bool wasLongPress =
      (now - frontButtonPressedSinceMs) >= FRONT_STROBE_HOLD_MS;

    if (!wasLongPress) {
      nextFrontLightMode();
    }
  }

  frontButtonWasPressed = frontPressed;
}

void updateBrakeLightHold() {
  const bool brakePressed = brakeButton.isPressed();
  const unsigned long now = millis();

  if (brakePressed && !brakeButtonWasPressed) {
    brakeButtonPressedSinceMs = now;
  }

  brakeButtonWasPressed = brakePressed;

  if (brakeButton.isPressed()) {
    brakeLightActive = true;
    brakeLightLastPressedMs = now;
  }

  if (brakeLightActive &&
      !brakeButton.isPressed() &&
      (now - brakeLightLastPressedMs) >= BRAKE_LIGHT_MIN_ON_MS) {
    brakeLightActive = false;
  }
}

bool isBrakeLightOn() {
  return brakeLightActive || brakeButton.isPressed();
}

bool isAdaptiveBrakeBlinking() {
  return brakeButton.isPressed() &&
         (millis() - brakeButtonPressedSinceMs) >= ADAPTIVE_BRAKE_HOLD_MS;
}

bool isFrontStrobeBlinking() {
  return frontButton.isPressed() &&
         (millis() - frontButtonPressedSinceMs) >= FRONT_STROBE_HOLD_MS;
}

void applyOutputs() {
  analogWrite(PIN_PWM_FRONT, frontPwmValue());
  analogWrite(PIN_PWM_REAR, rearPwmValue());
}

byte frontPwmValue() {
  if (isFrontStrobeBlinking()) {
    const unsigned long blinkStep =
      ((millis() - frontButtonPressedSinceMs - FRONT_STROBE_HOLD_MS) /
       FRONT_STROBE_BLINK_MS);

    if ((blinkStep % 2) == 0) {
      return PWM_FRONT_FULL;
    }

    return PWM_FRONT_OFF;
  }

  switch (frontLightMode) {
    case FRONT_OFF:
      return PWM_FRONT_OFF;

    case FRONT_DAYLIGHT:
      return PWM_FRONT_DAYLIGHT;

    case FRONT_FULL:
    default:
      return PWM_FRONT_FULL;
  }
}

byte rearPwmValue() {
  if (isAdaptiveBrakeBlinking()) {
    const unsigned long blinkStep =
      ((millis() - brakeButtonPressedSinceMs - ADAPTIVE_BRAKE_HOLD_MS) /
       ADAPTIVE_BRAKE_BLINK_MS);

    if ((blinkStep % 2) == 0) {
      return PWM_REAR_BRAKE;
    }

    return PWM_REAR_NORMAL;
  }

  if (isBrakeLightOn()) {
    return PWM_REAR_BRAKE;
  }

  return PWM_REAR_NORMAL;
}
