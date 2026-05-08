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

Aktiv fuer die Fahrzeuglogik werden nur diese Buttons genutzt:

| Receiver-Button | Host-Button | Funktion |
|---|---|---|
| 0 | 0 | Down Shift |
| 1 | 1 | Up Shift |
| 2 | 8 | R2 |
| 3 | 9 | L2 |
| 4 | 10 | L1 |
| 5 | 11 | R1 |
| 6 | 12 | PS |

Die Host-Buttons `2..7` werden aktuell nicht in die Fahrzeuglogik uebernommen.

## CRSF-Kanalbelegung

Der ESP32-Sender erzeugt ein standardkonformes CRSF-RC-Frame mit 16 Kanaelen:

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
| CH10 / Index 9 | Receiver-Button 6 = PS |
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
- Fahrstufen `R / N / D`
- Sicherheits-Neutral ueber `PS`
- Freigabe von `N` ueber `L1 + R1`
- Umschalten zwischen `D` und `R` ueber die Shift-Paddles
- erneutes `L1 + R1` wird nach der Freigabe ignoriert
- Fahrmodus `Normal / Sport`
- in `Sport`: lineare Gas- und Lenkkennlinie
- in `Normal`: progressive Gas- und progressive Lenkkennlinie
- Rueckwaerts-Geschwindigkeitslimit
- Kameraumschaltung mit `R2`
- invertierte Lenkung bei aktiver Rueckfahrkamera
- `R1` tippen: Hauptlicht ein/aus
- `L1` tippen: Lichthupe
- Bremslicht aktiv bei gedrueckter Bremse
- Failsafe nach `500 ms`

## Bedienung

Startzustand:

- Fahrzeug startet in `N`
- `N` ist verriegelt
- ESC bekommt Neutral

Schalten:

- `L1 + R1` innerhalb des Zeitfensters -> `N` freigeben
- `Up Shift` -> `D`
- `Down Shift` -> `R`
- in `D` und `R` schalten die Paddles direkt zwischen den Fahrstufen
- zurueck nach `N` geht nur noch ueber `PS`
- `PS` -> sofortiges Sicherheits-`N`

Fahrmodi:

- `L2` schaltet zwischen `Normal` und `Sport`
- `Normal` = progressive Gas- und Lenkkennlinie
- `Sport` = lineare Gas- und Lenkkennlinie

Kamera:

- `R2` schaltet zwischen Front- und Rueckansicht
- bei aktiver Rueckansicht wird die Lenkung invertiert

Licht:

- `R1` tippen -> Hauptlicht ein/aus
- `L1` tippen -> Lichthupe
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
<Gear>|<Mode>|L<0/1>|C<0/1>|T<degC>
```

Beispiele:

- `N|NORMAL|L0|C0|T28`
- `D|SPORT|L1|C0|T31`
- `R|NORMAL|L1|C1|T34`

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
- `LIGHTS_COMBO_WINDOW_MS`
- `LIGHTS_FLASH_DURATION_MS`

## Relevante Code-Dateien

- `Core/Src/app.c`
- `Core/Src/crsf_receiver.c`
- `Core/Src/crsf_telemetry.c`
- `Core/Src/sensor_data.c`
- `Core/Src/rc_state.c`
- `Core/Src/vehicle_control.c`
- `Core/Src/drive_pwm.c`
- `Core/Src/lights_control.c`
