#include <Servo.h>

// Board-Test fuer klassischen Arduino Nano (ATmega328P)
// Ziel:
// - Lenkservo-Ausgang auf Mittelstellung halten
// - Kamera-Switch auf A0 auf neutralen Puls stellen
// - Kamera-Schwenkservo auf D7 auf Mittelstellung halten
// - Lichtausgaenge sicher ausgeschaltet halten
//
// Wichtiger Hinweis:
// Der klassische Nano kann A6 NICHT als digitalen/Servo-Ausgang treiben.
// Auf deiner STM32-Platine liegt der ESC aber auf A6.
// Deshalb kann dieser Test den ESC-Ausgang auf der fertigen Platine nicht 1:1 bedienen.
// Falls du den ESC-Signalpfad trotzdem separat testen willst, kannst du optional
// per Jumper den unten angegebenen Test-Pin D6 auf das ESC-Signalnetz legen.

static constexpr uint8_t PIN_CAMERA_SWITCH = A0;
static constexpr uint8_t PIN_STEERING_SERVO = A5;
static constexpr uint8_t PIN_CAMERA_PAN_SERVO = 7;

static constexpr uint8_t PIN_LIGHT_MAIN = 12;
static constexpr uint8_t PIN_LIGHT_BRAKE = 11;

// Nur fuer optionalen Jumper-Test. Nicht mit der festen A6-Netzbelegung identisch.
static constexpr uint8_t PIN_ESC_TEST = 6;
static constexpr bool ENABLE_ESC_TEST_PIN = false;

static constexpr int SERVO_NEUTRAL_US = 1500;

Servo steeringServo;
Servo cameraSwitchServo;
Servo cameraPanServo;
Servo escTestServo;

static void setOutputsSafe(void)
{
  digitalWrite(PIN_LIGHT_MAIN, LOW);
  digitalWrite(PIN_LIGHT_BRAKE, LOW);
}

void setup()
{
  pinMode(PIN_LIGHT_MAIN, OUTPUT);
  pinMode(PIN_LIGHT_BRAKE, OUTPUT);
  setOutputsSafe();

  steeringServo.attach(PIN_STEERING_SERVO);
  cameraSwitchServo.attach(PIN_CAMERA_SWITCH);
  cameraPanServo.attach(PIN_CAMERA_PAN_SERVO);

  steeringServo.writeMicroseconds(SERVO_NEUTRAL_US);
  cameraSwitchServo.writeMicroseconds(SERVO_NEUTRAL_US);
  cameraPanServo.writeMicroseconds(SERVO_NEUTRAL_US);

  if (ENABLE_ESC_TEST_PIN)
  {
    escTestServo.attach(PIN_ESC_TEST);
    escTestServo.writeMicroseconds(SERVO_NEUTRAL_US);
  }
}

void loop()
{
  steeringServo.writeMicroseconds(SERVO_NEUTRAL_US);
  cameraSwitchServo.writeMicroseconds(SERVO_NEUTRAL_US);
  cameraPanServo.writeMicroseconds(SERVO_NEUTRAL_US);

  if (ENABLE_ESC_TEST_PIN)
  {
    escTestServo.writeMicroseconds(SERVO_NEUTRAL_US);
  }

  setOutputsSafe();
  delay(20);
}
