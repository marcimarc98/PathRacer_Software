# ESP32-S3 IDF Sendestation TX-Only

Minimaler ESP-IDF-Senderstand nur fuer den sicheren Funk-Hinweg.

Ziel:
- PC/App -> ESP32-S3 per USB Serial/JTAG
- ESP32-S3 -> ELRS-Sendermodul nur ueber `GPIO17` als TX
- keine Rueckkanal-Logik
- keine Telemetrieauswertung
- 250 Hz CRSF-RC-Frames

Verdrahtung:
- `GPIO17` -> CRSF-Datenleitung zum ELRS-Sendermodul
- `GPIO18` wird in diesem Projekt bewusst nicht verwendet

Dieses Projekt ist als Referenz-/Rettungsstand gedacht:
- Wenn dieser Stand wieder sauberen Hinkanal liefert, liegt das Problem im Split-RX/TX-Rueckkanalpfad
- Danach kann man den Telemetriepfad sauber wieder aufbauen
