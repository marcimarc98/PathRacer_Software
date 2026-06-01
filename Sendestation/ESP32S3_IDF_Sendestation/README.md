# ESP32-S3 IDF Sendestation

ESP-IDF-Port des funktionierenden Arduino-Referenzstands in
[VehicleGroundStation_Reference.ino](C:/Users/marc_/OneDrive/PathRacer/PMT2/Projektdurchfuehrung/Software/Sendestation/VehicleGroundStation_Reference/VehicleGroundStation_Reference.ino).

Ziel:
- gleiches Host-Paketformat wie die Windows-App
- gleiche CRSF-Kanalbelegung wie der Arduino-Stand
- gleicher Rueckkanal als Statuspaket `0x5A 0xA5 0x31 ...`
- gleiches Single-Wire-Verhalten auf der CRSF-Leitung

Verdrahtung:
- CRSF-Datenleitung wie im Arduino-Stand auf `GPIO17`
- die IDF-Version schaltet denselben Pin zum Senden kurz aktiv und geht danach wieder in Listen-Modus

Host/App:
- Kommunikation zur Windows-App laeuft ueber `USB Serial/JTAG`
- die App kann den Port weiter mit `460800` oeffnen

Build:

```bat
set IDF_PATH=C:\esp\v6.0.1\esp-idf
cd /d C:\Users\marc_\OneDrive\PathRacer\PMT2\Projektdurchfuehrung\Software\Sendestation\ESP32S3_IDF_Sendestation
cmake --build build
```

Flash:

```bat
set IDF_PATH=C:\esp\v6.0.1\esp-idf
cd /d C:\Users\marc_\OneDrive\PathRacer\PMT2\Projektdurchfuehrung\Software\Sendestation\ESP32S3_IDF_Sendestation
cmake --build build --target flash
```

Hinweis:
- die bisherige Idee mit getrennten Pins `GPIO17` und `GPIO18` ist damit bewusst noch nicht umgesetzt
- dieser Stand priorisiert 1:1-Verhalten zur aktuell funktionierenden Arduino-Referenz
