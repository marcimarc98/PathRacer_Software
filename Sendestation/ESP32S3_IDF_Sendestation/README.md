# ESP32-S3 IDF Sendestation

Eigenstaendiger ESP-IDF-Senderstand fuer den ESP32-S3.

Ziel:
- gleiche Host-Paketstruktur wie die bestehende App
- gleiche CRSF-Kanalbelegung wie der funktionierende Arduino-Stand
- Rueckkanal weiter als Statuspaket `0x5A 0xA5 0x31 ...` an die Windows-App
- CRSF-Leitung am Sendermodul elektrisch auf zwei ESP-Pins aufteilen:
  - `GPIO17` = TX vom ESP zum ELRS-Sendermodul
  - `GPIO18` = RX vom ELRS-Sendermodul zum ESP

Verdrahtung:
- `eine` CRSF-Leitung des Sendermoduls auf `GPIO17` und `GPIO18` gleichzeitig splitten
- `GPIO17` sendet nur aktiv waehrend eines RC-Frames
- `GPIO18` lauscht dauerhaft auf Telemetrie
- spaeter kann optional ein Serienwiderstand im TX-Zweig ergaenzt werden

USB:
- Host/App Kommunikation laeuft ueber `USB Serial/JTAG`
- Baud fuer die App bleibt `460800`

Build (Windows CMD):

```bat
call C:\esp\v6.0.1\esp-idf\export.bat
cd /d C:\Users\marc_\OneDrive\PathRacer\PMT2\Projektdurchfuehrung\Software\Sendestation\ESP32S3_IDF_Sendestation
C:\Espressif\tools\idf-exe\1.0.3\idf.py.exe set-target esp32s3
C:\Espressif\tools\idf-exe\1.0.3\idf.py.exe build
```

Flash:

```bat
call C:\esp\v6.0.1\esp-idf\export.bat
cd /d C:\Users\marc_\OneDrive\PathRacer\PMT2\Projektdurchfuehrung\Software\Sendestation\ESP32S3_IDF_Sendestation
C:\Espressif\tools\idf-exe\1.0.3\idf.py.exe -p COM11 flash
```
