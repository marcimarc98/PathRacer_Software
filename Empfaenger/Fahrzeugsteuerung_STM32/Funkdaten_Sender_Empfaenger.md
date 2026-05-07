# Funkdaten Sender / Empfaenger

Kurzbeschreibung des aktuellen Systems:

- Python liest Lenkrad und Pedale am PC aus
- der ESP32-Sender baut daraus ein CRSF-RC-Frame
- der STM32-Empfaenger dekodiert das CRSF-Frame
- der STM32 erzeugt daraus Servo- und ESC-PWM fuer das Fahrzeug

## Pin-Belegung STM32

Board:

- NUCLEO-F303K8
- MCU: STM32F303K8T6

Aktuell verwendete Pins:

| Funktion | STM32-Pin | Nucleo-Anschluss |
|---|---|---|
| CRSF Empfang RX | `PA10` | `D0`, `CN3 Pin 2` |
| Servo PWM | `PA6` | `A5`, `CN4 Pin 7` |
| ESC PWM | `PA7` | `A6`, `CN4 Pin 6` |
| SWDIO | `PA13` | SWD Debug |
| SWCLK | `PA14` | SWD Debug |
| MCO / OSC_IN | `PF0` | `D7`, `CN3 Pin 10` |
| User LED LD3 | `PB3` | `D13`, `CN4 Pin 15` |

## Host-Paket PC -> ESP32

Das Python-Programm sendet ein festes 11-Byte-Paket:

| Byte(s) | Inhalt | Typ |
|---|---|---|
| 0 | `0xAA` | Header |
| 1 | `0x55` | Header |
| 2-3 | Lenkung | `int16`, little endian |
| 4-5 | Gas | `uint16`, little endian |
| 6-7 | Bremse | `uint16`, little endian |
| 8-9 | Buttons | `uint16`, little endian |
| 10 | XOR Checksumme | `uint8` |

Format:

```text
<BBhHHH + checksum
```

## Eingelesene Lenkrad-Daten

Python liest:

- `Axis 0` = Lenkung
- `Axis 1` = Bremse
- `Axis 2` = Gas
- `Button 0..12`

Skalierung:

- Lenkung: `-1.0 .. +1.0` -> `-1000 .. +1000`
- Gas: `0.0 .. 1.0` -> `0 .. 1000`
- Bremse: `0.0 .. 1.0` -> `0 .. 1000`
- Buttons: als Bits in einem `uint16`

## Button-Belegung am Lenkrad

Originale Host-Buttons:

| Host-Button | Funktion |
|---|---|
| 0 | Down Shift |
| 1 | Up Shift |
| 2 | Dreieck |
| 3 | Kreis |
| 4 | Viereck |
| 5 | X |
| 6 | Drehknopf nach Links |
| 7 | Drehknopf nach Rechts |
| 8 | R2 |
| 9 | L2 |
| 10 | L1 |
| 11 | R1 |
| 12 | PS Button |

Aktiv genutzt werden im aktuellen Fahrzeugcode nur:

| Receiver-Button | Quelle |
|---|---|
| 0 | Down Shift |
| 1 | Up Shift |
| 2 | R2 |
| 3 | L2 |
| 4 | L1 |
| 5 | R1 |
| 6 | PS Button |

Die Host-Buttons `2..7` werden aktuell nicht in die Fahrzeuglogik uebernommen.

## CRSF-Kanalbelegung

Der ESP32-Sender erzeugt ein standardkonformes CRSF-RC-Frame mit 16 Kanaelen.

Aktuelle Belegung:

| CRSF-Kanal | Inhalt |
|---|---|
| CH1 / Index 0 | Lenkung |
| CH2 / Index 1 | Gas |
| CH3 / Index 2 | Bremse |
| CH4 / Index 3 | Receiver-Button 0 = Down Shift |
| CH5 / Index 4 | Receiver-Button 1 = Up Shift |
| CH6 / Index 5 | Receiver-Button 2 = R2 |
| CH7 / Index 6 | Receiver-Button 3 = L2 |
| CH8 / Index 7 | Receiver-Button 4 = L1 |
| CH9 / Index 8 | Receiver-Button 5 = R1 |
| CH10 / Index 9 | Receiver-Button 6 = PS Button |
| CH11 / Index 10 | unbenutzt |
| CH12 / Index 11 | unbenutzt |
| CH13 / Index 12 | unbenutzt |
| CH14 / Index 13 | unbenutzt |
| CH15 / Index 14 | unbenutzt |
| CH16 / Index 15 | unbenutzt |

CRSF-Werte:

- analog: CRSF-Min/Max-Bereich
- digital:
  - `false` -> CRSF Minimum
  - `true` -> CRSF Maximum

## CRSF <-> us Mapping

Lenkung, Gas und Bremse werden im Empfaenger in Pulsweiten umgerechnet:

- CRSF Mitte `992` entspricht etwa `1500 us`
- kleiner als Mitte = unter `1500 us`
- groesser als Mitte = ueber `1500 us`

Digitale Buttons werden im Empfaenger als gedrueckt erkannt, wenn der Kanalwert oberhalb der Mitte liegt.

## Implementierte Funktionen im Fahrzeugcode

Aktuell implementiert:

- CRSF-Empfang auf `USART1`
- Speichern des kompletten RC-Zustands im RAM
- PWM-Ausgabe fuer Servo und ESC auf `TIM3`
- Fahrstufen `R / N / D`
- Sicherheits-Neutral ueber `PS`
- Entsperren von `N` ueber `L1 + R1`
- Umschalten zwischen `D` und `R` ueber die Shift-Paddles
- Bremse nur in `D`
- Bremse in `R` komplett deaktiviert
- Rueckwaerts-Geschwindigkeitslimit
- Fahrmodus `Normal / Sport`
- progressive Gaskennlinie in `Normal`
- lineare Gaskennlinie in `Sport`
- progressive Lenk-Expo ohne Deadzone
- Failsafe nach `500 ms`

## Bedienung

Startzustand:

- Fahrzeug startet in `N`
- `N` ist verriegelt
- ESC bekommt Neutral

Freigabe:

- `L1 + R1` gleichzeitig druecken
- danach ist `N` freigegeben

Schalten:

- `Up Shift` -> `D`
- `Down Shift` -> `R`
- direktes Umschalten zwischen `D` und `R` ist moeglich

Zurueck nach Neutral:

- `L1 + R1` gleichzeitig druecken -> verriegeltes `N`
- `PS` -> sofortiges Sicherheits-`N`

Fahrmodi:

- `L2` schaltet zwischen `Normal` und `Sport`

Verhalten der Pedale:

- in `N`: kein Antrieb
- in `D`: Gas vorwaerts, Bremse bremst
- in `R`: Gas rueckwaerts, Bremse wird ignoriert

## Wichtige Stellschrauben im Code

Datei:

- `Core/Src/vehicle_control.c`

Wichtige Konstanten:

- `REVERSE_GAS_LIMIT_PERCENT`
- `NORMAL_DRIVE_PROGRESSIVITY_PERCENT`
- `STEERING_EXPO_PERCENT`
- `GAS_ACTIVE_DEADBAND_US`
- `BRAKE_ACTIVE_DEADBAND_US`

## Relevante Code-Dateien

- `Schnittstellendoku_Sender/Lenkraddaten_einlesen_und_an_ESP_senden.py`
- `Schnittstellendoku_Sender/Sendestation.ino`
- `Core/Src/app.c`
- `Core/Src/crsf_receiver.c`
- `Core/Src/rc_state.c`
- `Core/Src/vehicle_control.c`
- `Core/Src/drive_pwm.c`
