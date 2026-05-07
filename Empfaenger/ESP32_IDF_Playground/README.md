# ESP32 IDF Playground

Kleines ESP-IDF-Startprojekt fuer einen klassischen ESP32-WROOM-32 mit Blink-LED auf `GPIO2`.

## Voraussetzungen

- ESP-IDF v6.0.1 ist lokal installiert.
- Serieller Port ist aktuell `COM6`.
- VS Code hat die Erweiterungen `ESP-IDF` und `C/C++`.

## VS Code

- Build: `Ctrl+Shift+B` oder Task `ESP-IDF: Build`
- Flash: Task `ESP-IDF: Flash`
- Monitor: Task `ESP-IDF: Monitor`
- Build + Flash + Monitor: Task `ESP-IDF: Build Flash Monitor`
- Debug: Launch `ESP-IDF JTAG Debug (ESP32)`

## Debug-Hinweis

Fuer klassisches Hardware-Debugging auf einem ESP32-WROOM-32 brauchst du zusaetzlich einen JTAG-Debugger, zum Beispiel `ESP-Prog`. Die Launch-Konfiguration ist fuer die uebliche `FTDI/ESP-Prog`-Variante mit `3.3V` vorbereitet. Falls du einen anderen JTAG-Adapter nutzt, musst du in `.vscode/settings.json` nur `idf.openOcdConfigs` anpassen.
