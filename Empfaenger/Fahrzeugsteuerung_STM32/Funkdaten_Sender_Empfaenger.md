# Funkdaten Sender / Empfaenger

## Uebersicht

Aktueller Datenweg:

- Die Windows-App `Vehicle Ground Station` liest Lenkrad, Gas, Bremse und Buttons am PC.
- Die App sendet ein festes Host-Paket per USB-Serial an den ESP32-Sender.
- Der ESP32-Sender baut daraus ein CRSF-RC-Frame und gibt es an das ELRS-Sendermodul aus.
- Der STM32 im Fahrzeug dekodiert das CRSF-RC-Frame und erzeugt Servo-, ESC-, Kamera- und Lichtsignale.
- Der STM32 baut zusaetzlich CRSF-Telemetrie fuer den Rueckkanal.
- Der ESP32-Sender liest diese Rueckdaten wieder ein und gibt einen kompakten Status an die Windows-App zurueck.

## Pin-Belegung STM32

Board:

- `NUCLEO-F303K8`
- MCU: `STM32F303K8T6`

Aktiv verwendete Pins:

| Funktion | STM32-Pin | Nucleo-Anschluss |
|---|---|---|
| Kamera Switch vorne/hinten | `PA0` | `A0`, `CN4 Pin 1` |
| Akku-Temp-ADC | `PA1` | `A1`, `CN4 Pin 2` |
| Akku-ADC | `PA3` | `A2`, `CN4 Pin 3` |
| CRSF RX vom ELRS-Empfaenger | `PA10` | `D0`, `CN3 Pin 2` |
| CRSF TX zum ELRS-Empfaenger | `PA9` | `D1`, `CN3 Pin 1` |
| Lenkservo PWM | `PA6` | `A5`, `CN4 Pin 7` |
| ESC PWM | `PA7` | `A6`, `CN4 Pin 6` |
| Diff vorne PWM | `PB0` | `D3` |
| Diff hinten PWM | `PB1` | `D6` |
| Kamera-Schwenkservo PWM | `PA8` | `D9` |
| Hauptlicht | `PB4` | `D12`, `CN4 Pin 14` |
| Bremslicht | `PB5` | `D11`, `CN4 Pin 13` |
| SWDIO | `PA13` | SWD Debug |
| SWCLK | `PA14` | SWD Debug |

Akku-Spannungsteiler fuer `4S Li-Ion`:

- oberer Widerstand von Akku `+` nach `A2/PA3`: `47 kOhm`
- unterer Widerstand von `A2/PA3` nach Akku `GND`: `10 kOhm`

Akku-Temperatursensor:

- Sensor: `MF52 10k NTC`, ausgelegt auf die uebliche `B3950`-Kennlinie
- Pullup von `3.3V` nach `A1/PA1`: `3.9 kOhm`
- NTC von `A1/PA1` nach `GND`
- Kennlinie im Code fuer `20-80 C` per Tabelle auf den MF52-Daten interpoliert

## Servo-PWM-Pins

Aktuell genutzt und fuer Erweiterungen vorgesehen:

| Funktion | Arduino-Pin | STM32-Pin | Timerfunktion |
|---|---|---|---|
| Kamera Switch vorne/hinten | `A0` | `PA0` | `TIM2_CH1` |
| Akku-Temperatur | `A1` | `PA1` | ADC |
| Akku-Spannung | `A2` | `PA3` | ADC |
| Lenkservo | `A5` | `PA6` | `TIM3_CH1` |
| ESC | `A6` | `PA7` | `TIM3_CH2` |
| Diff vorne | `D3` | `PB0` | `TIM3_CH3` |
| Diff hinten | `D6` | `PB1` | `TIM3_CH4` |
| Kamera-Schwenkservo | `D9` | `PA8` | `TIM1_CH1` |

## Host-Paket PC -> ESP32

Die App sendet ein festes 11-Byte-Paket:

| Byte(s) | Inhalt | Typ |
|---|---|---|
| 0 | `0xAA` | Header |
| 1 | `0x55` | Header |
| 2-3 | Lenkung | `int16`, little endian |
| 4-5 | Gas | `uint16`, little endian |
| 6-7 | Bremse | `uint16`, little endian |
| 8-9 | Buttons | `uint16`, little endian |
| 10 | XOR-Checksumme | `uint8` |

Feste Reihenfolge:

```text
Lenkung -> Gas -> Bremse -> Buttons
```

## Lenkrad- und Button-Daten

Feste Zuordnung in der App:

- `Axis 0 / X` = Lenkung
- `Axis 1 / Y` = Bremse
- `Axis RZ` = Gas
- `Button 0..12` = Host-Buttons

Neue direkte Bedienung:

- `Button 7` = Kamera +30 Grad
- `Button 6` = Kamera -30 Grad
- `Button 3` = Diff vorne sperren
- `Button 5` = Diff vorne entsperren
- `Button 2` = Diff hinten sperren
- `Button 4` = Diff hinten entsperren

Die App verarbeitet die physischen Lenkrad-Buttons lokal zu fertigen Soll-Zustaenden.
An ESP32 und STM32 geht nur noch ein gepacktes 11-Bit-Steuerwort:

| Host-Feld | Funktion |
|---|---|
| Bit 0 | Sollzustand Rueckwaerts |
| Bit 1 | Sollzustand Vorwaerts |
| Bit 2 | Kamera hinten aktiv |
| Bit 3 | Normalmodus aktiv |
| Bit 4 | Lichthupe aktiv |
| Bit 5 | Hauptlicht ein |
| Bit 6 | Diff vorne gesperrt |
| Bit 7 | Diff hinten gesperrt |
| Bit 8..10 | Kamera-Schwenkwinkel-Code |

Kamera-Schwenkwinkel-Code:

| Code | Winkel |
|---|---|
| 1 | `-90 Grad` |
| 2 | `-60 Grad` |
| 3 | `-30 Grad` |
| 4 | `0 Grad` |
| 5 | `+30 Grad` |
| 6 | `+60 Grad` |
| 7 | `+90 Grad` |

Code `0` wird nicht als Sollwert erzeugt und am Empfaenger wie Mittelstellung behandelt.

## CRSF-Kanalbelegung

Der ESP32-Sender erzeugt ein standardkonformes CRSF-RC-Frame mit 16 Kanaelen:

| CRSF-Kanal | Inhalt |
|---|---|
| CH1 / Index 0 | Lenkung |
| CH2 / Index 1 | Gas |
| CH3 / Index 2 | Bremse |
| CH4 / Index 3 | segmentiertes Steuerwortsymbol |
| CH5..CH16 | unbenutzt, auf CRSF Minimum |

Das logische Steuerwort entspricht dem Host-Buttonwort:

- Bits `0..7`: logische Fahrzeugzustaende
- Bits `8..10`: Kamera-Schwenkwinkel-Code

Dieses 11-Bit-Steuerwort wird nicht direkt als Rohwert auf `CH4` gelegt, weil die unteren
Kanalbits ueber ELRS nicht stabil genug fuer digitale Schaltbits sind. Der ESP32 sendet
stattdessen auf `CH4` zyklisch ein robustes Symbol:

- 4 Segmente mit je 3 Nutzbits
- Symbolnummer = `(Segment << 3) | Payload`
- CRSF-Wert = `220 + Symbolnummer * 48`

Der STM32 setzt daraus wieder das 11-Bit-Steuerwort zusammen. Es werden weiterhin keine
separaten CRSF-Kanaele fuer einzelne Buttons genutzt.

## CRSF <-> us Mapping

Im Empfaenger werden Lenkung, Gas und Bremse auf Pulsweiten umgerechnet:

- CRSF Mitte `992` entspricht etwa `1500 us`
- kleiner als Mitte = unter `1500 us`
- groesser als Mitte = ueber `1500 us`

Das Steuerwort wird aus den `CH4`-Segmenten zusammengesetzt:

- untere 8 Bits = logische Fahrzeugzustaende
- obere 3 Bits = Kamera-Schwenkwinkel-Code

## Implementierte Fahrzeugfunktionen

Im aktuellen STM32-Stand sind implementiert:

- CRSF Voll-Duplex auf `USART1`
- Speichern des kompletten RC-Zustands im RAM
- Servo-PWM auf `PA6`
- ESC-PWM auf `PA7`
- Kamera-Switch-PWM auf `PA0`
- Kamera-Schwenkservo auf `PA8`
- Diff vorne auf `PB0`
- Diff hinten auf `PB1`
- Hauptlichtausgang auf `PB4`
- Bremslichtausgang auf `PB5`
- Akku-Spannungsmessung per ADC auf `PA3`
- Akku-Temperaturmessung per ADC auf `PA1`
- zentrales Sensordatenmodul fuer Spannung und Temperatur
- Akku-Spannung wird lokal ueber mehrere schnelle Messungen gemittelt und danach in den Rueckkanal gegeben
- Akku-Temperatur wird direkt gemessen und nicht zusaetzlich gemittelt
- zustandsgetriebene Fahrstufen `R / N / D`
- zustandsgetriebener Fahrmodus `Aggressiv / Normal`
- in `Aggressiv`: lineare Gas- und Lenkkennlinie
- in `Normal`: progressive Gas- und progressive Lenkkennlinie
- Rueckwaerts-Geschwindigkeitslimit
- zustandsgetriebene Kamera vorne/hinten
- zustandsgetriebener Kamera-Schwenkwinkel von `-90` bis `+90 Grad`
- invertierte Lenkung bei aktiver Rueckfahrkamera
- zustandsgetriebene Diff-Sperre vorne
- zustandsgetriebene Diff-Sperre hinten
- zustandsgetriebenes Hauptlicht
- zustandsgetriebene Lichthupe
- Bremslicht aktiv bei gedrueckter Bremse
- Failsafe nach `500 ms`
- Sicherheitslogik fuer den Antrieb bleibt lokal im STM32

## Bedienung

Die App verarbeitet die physische Bedienung lokal und sendet nur fertige Soll-Zustaende.
Der STM32 setzt diese Zustande direkt um und behaelt nur die Sicherheitslogik fuer den Antrieb:

- `Sollzustand Vorwaerts = 1` -> `D`
- `Sollzustand Rueckwaerts = 1` -> `R`
- kein Richtungsbit -> verriegeltes `N`
- `Normalmodus aktiv = 1` -> `Normal`, sonst `Aggressiv`
- `Kamera hinten aktiv = 1` -> Rueckfahrkamera und invertierte Lenkung
- `Kamera-Schwenkwinkel-Code` -> Kamera-Schwenkservo in 30-Grad-Schritten
- `Diff vorne gesperrt = 1` -> vordere Diff-Sperre aktiv
- `Diff hinten gesperrt = 1` -> hintere Diff-Sperre aktiv
- `Hauptlicht ein = 1` -> Hauptlicht an
- `Lichthupe aktiv = 1` -> Lichtausgang zusaetzlich aktiv
- Bremse gedrueckt -> Bremslicht an

Pedalverhalten:

- in `N`: kein Antrieb
- in `D`: Gas vorwaerts, Bremse bremst
- in `R`: Gas rueckwaerts, Bremse wird fuer den ESC ignoriert

## Rueckkanal

Der STM32 baut zwei standardkonforme CRSF-Telemetrie-Frames:

- `0x21 Flight Mode`
- `0x08 Battery Sensor`

Aktueller Textinhalt im `0x21 Flight Mode`-Frame:

```text
T<degC>
```

Beispiel:

- `T31`

Bedeutung:

- `Gear` = `R`, `N`, `D`
- `Mode` = `Aggressiv` oder `Normal`
- `L` = Hauptlicht aus/ein
- `C` = Front-/Rueckkamera
- `T` = Akku-Temperatur in `°C`

Zusaetzlich sendet `0x08 Battery Sensor`:

- Akku-Spannung
- Akku-Restwert in Prozent

## Wichtige Stellschrauben im Code

Dateien:

- `Core/Src/vehicle_control.c`
- `Core/Src/drive_pwm.c`
- `Core/Src/lights_control.c`
- `Core/Src/sensor_data.c`

Wichtige Konstanten:

- `REVERSE_GAS_LIMIT_PERCENT`
- `NORMAL_DRIVE_PROGRESSIVITY_PERCENT`
- `STEERING_EXPO_PERCENT`
- `GAS_ACTIVE_DEADBAND_US`
- `BRAKE_ACTIVE_DEADBAND_US`
- `CAMERA_PWM_FRONT_US`
- `CAMERA_PWM_REAR_US`
- `CAMERA_PAN_CENTER_US`
- `CAMERA_PAN_RANGE_US`
- `DIFF_UNLOCKED_US`
- `DIFF_LOCKED_US`

## Relevante Code-Dateien

- `Core/Src/app.c`
- `Core/Src/crsf_receiver.c`
- `Core/Src/crsf_telemetry.c`
- `Core/Src/sensor_data.c`
- `Core/Src/rc_state.c`
- `Core/Src/vehicle_control.c`
- `Core/Src/drive_pwm.c`
- `Core/Src/lights_control.c`
