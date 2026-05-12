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
| Kamera PWM | `PA0` | `A0`, `CN4 Pin 1` |
| Akku-Temp-ADC | `PA1` | `A1`, `CN4 Pin 2` |
| Akku-ADC | `PA3` | `A2`, `CN4 Pin 3` |
| CRSF RX vom ELRS-Empfaenger | `PA10` | `D0`, `CN3 Pin 2` |
| CRSF TX zum ELRS-Empfaenger | `PA9` | `D1`, `CN3 Pin 1` |
| Servo PWM | `PA6` | `A5`, `CN4 Pin 7` |
| ESC PWM | `PA7` | `A6`, `CN4 Pin 6` |
| Hauptlicht | `PB4` | `D12`, `CN4 Pin 14` |
| Bremslicht | `PB5` | `D11`, `CN4 Pin 13` |
| SWDIO | `PA13` | SWD Debug |
| SWCLK | `PA14` | SWD Debug |

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

Die App verarbeitet die physischen Lenkrad-Buttons lokal zu fertigen Soll-Zustaenden.
An ESP32 und STM32 gehen nur noch diese logischen Zustandsbits:

| Receiver-Button | Host-Bit | Funktion |
|---|---|---|
| 0 | 0 | Sollzustand Rueckwaerts |
| 1 | 1 | Sollzustand Vorwaerts |
| 2 | 2 | Kamera hinten aktiv |
| 3 | 3 | Sportmodus aktiv |
| 4 | 4 | Lichthupe aktiv |
| 5 | 5 | Hauptlicht ein |
| 6 | 6 | Neutral freigegeben |

## CRSF-Kanalbelegung

Der ESP32-Sender erzeugt ein standardkonformes CRSF-RC-Frame mit 16 Kanaelen:

| CRSF-Kanal | Inhalt |
|---|---|
| CH1 / Index 0 | Lenkung |
| CH2 / Index 1 | Gas |
| CH3 / Index 2 | Bremse |
| CH4 / Index 3 | Sollzustand Rueckwaerts |
| CH5 / Index 4 | Sollzustand Vorwaerts |
| CH6 / Index 5 | Kamera hinten aktiv |
| CH7 / Index 6 | Sportmodus aktiv |
| CH8 / Index 7 | Lichthupe aktiv |
| CH9 / Index 8 | Hauptlicht ein |
| CH10 / Index 9 | Neutral freigegeben |
| CH11..CH16 | unbenutzt |

Buttons werden digital uebertragen:

- `false` -> CRSF Minimum
- `true` -> CRSF Maximum

## CRSF <-> us Mapping

Im Empfaenger werden Lenkung, Gas und Bremse auf Pulsweiten umgerechnet:

- CRSF Mitte `992` entspricht etwa `1500 us`
- kleiner als Mitte = unter `1500 us`
- groesser als Mitte = ueber `1500 us`

Die Buttons werden als gedrueckt erkannt, wenn der Kanalwert oberhalb der Mitte liegt.

## Implementierte Fahrzeugfunktionen

Im aktuellen STM32-Stand sind implementiert:

- CRSF Voll-Duplex auf `USART1`
- Speichern des kompletten RC-Zustands im RAM
- Servo-PWM auf `PA6`
- ESC-PWM auf `PA7`
- Kamera-PWM auf `PA0`
- Hauptlichtausgang auf `PB4`
- Bremslichtausgang auf `PB5`
- Akku-Spannungsmessung per ADC auf `PA3`
- Akku-Temperaturmessung per ADC auf `PA1`
- zentrales Sensordatenmodul fuer Spannung und Temperatur
- Akku-Spannung wird lokal ueber mehrere schnelle Messungen gemittelt und danach in den Rueckkanal gegeben
- Akku-Temperatur wird direkt gemessen und nicht zusaetzlich gemittelt
- zustandsgetriebene Fahrstufen `R / N / D`
- zustandsgetriebener Fahrmodus `Normal / Sport`
- in `Sport`: lineare Gas- und Lenkkennlinie
- in `Normal`: progressive Gas- und progressive Lenkkennlinie
- Rueckwaerts-Geschwindigkeitslimit
- zustandsgetriebene Kamera vorne/hinten
- invertierte Lenkung bei aktiver Rueckfahrkamera
- zustandsgetriebenes Hauptlicht
- zustandsgetriebene Lichthupe
- Bremslicht aktiv bei gedrueckter Bremse
- Failsafe nach `500 ms`
- Sicherheitslogik fuer den Antrieb bleibt lokal im STM32

## Bedienung

Die App verarbeitet die physische Bedienung lokal und sendet nur fertige Soll-Zustaende.
Der STM32 setzt diese Zustande direkt um und behaelt nur die Sicherheitslogik fuer den Antrieb:

- `Neutral freigegeben = 0` -> Fahrzeug bleibt verriegelt in `N`
- `Neutral freigegeben = 1` und kein Richtungsbit -> `N` freigegeben
- `Sollzustand Vorwaerts = 1` -> `D`
- `Sollzustand Rueckwaerts = 1` -> `R`
- `Sportmodus aktiv = 1` -> `Sport`, sonst `Normal`
- `Kamera hinten aktiv = 1` -> Rueckfahrkamera und invertierte Lenkung
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
- `Mode` = `NORMAL` oder `SPORT`
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

## Relevante Code-Dateien

- `Core/Src/app.c`
- `Core/Src/crsf_receiver.c`
- `Core/Src/crsf_telemetry.c`
- `Core/Src/sensor_data.c`
- `Core/Src/rc_state.c`
- `Core/Src/vehicle_control.c`
- `Core/Src/drive_pwm.c`
- `Core/Src/lights_control.c`
