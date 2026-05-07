# Software

Software-Basis fuer die Fahrzeugplattform mit Sender- und Empfaengerseite.

## Struktur

- `Sendestation/`
  PC- und ESP32-Seite fuer Lenkrad, Host-Paket und CRSF-Senderpfad.

- `Empfaenger/`
  STM32- und weitere Empfaengerprojekte fuer Fahrzeugsteuerung und Funkempfang.

- `CRSF_Handbuch.md`
  Zentrale Nachlese zur verwendeten CRSF-Schnittstelle.

## Repo-Hinweise

- Build- und Tool-Artefakte sind ueber die Root-`.gitignore` ausgeschlossen.
- Der Root enthaelt bewusst nur allgemeine Repo-Basisdateien.
- Projektbezogene Dokumentation und Build-Konfigurationen bleiben in den jeweiligen Unterordnern.
