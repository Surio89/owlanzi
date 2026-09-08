# Release 1.0.3 — Prüfbeleg

Stand: 7. September 2026. Alle 13 Befunde aus `FIRMWARE_REVIEW.md` sowie die
zusätzlichen Softwarepunkte sind im aktuellen Quellcode bearbeitet.
Änderungsübersicht: `CHANGELOG.md`. Wiederholbarer Web-Release: `DEPLOYMENT.md`.

## Validierung

- 98 native C++-Prüffälle: echte Produktionsfunktionen für Parsing, Schlafstatus,
  Frische, Alarme, Quittierung, Vorschau, Konfiguration, Upload und Cloud-Requests.
  Enthält UTC-/Timer-Grenzen, Geräteauswahl, APP_ACTIVE-Fehler und verworfene
  Antworten nach einem Kontowechsel. Hardware, Zeit und Netzwerk sind simuliert.
- 3 Browsertests: Ausfallanzeige, veraltete Spiegelantwort, keine anwachsende
  Anzahl paralleler Spiegelanfragen. Beide vollständigen eingebetteten
  Browserprogramme zusätzlich auf JavaScript-Syntax geprüft.
- 30 Website-Tests, 6 Firmware-Image-Audittests, 7 Deploymenttests. Die
  Deploymenttests führen die echten Shell-Kommandos in lokalen Testordnern aus
  und prüfen Sicherung sowie Abbruch bei konkurrierenden Änderungen.
- Release-/TC001-/ESP32dev-Builds erfolgreich. RAM 53.440 von 327.680 Bytes;
  Programm 1.227.733 von 1.966.080 Bytes pro OTA-Slot.
- cppcheck: alle 6 Implementierungsdateien, warning/performance/portability,
  ohne Befund. Website-Prüfung: 10 HTML-Seiten, 387 lokale Verweise und
  Quellstand-/Image-Prüfsummen erfolgreich.
- Beide Installer: 4 MB, DIO/40 MHz, zwei passende OTA-Slots, Bild- und
  Partitionstabellen-Prüfsummen korrekt. Drei private Seed-Werte geprüft;
  keine enthalten. `NO_LOCAL_SECRETS` aktiv.
- Persönlicher Skill `owlanzi-web-release` erstellt und Validator bestanden.

## Fertige Images

| Target | Datei | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| TC001 | `owlanzi-tc001-1.0.3.bin` | 1.299.840 | `ae1ba0159775dff98e463f4a023d4b973373ce593709f815f120259c454d51af` |
| ESP32dev | `owlanzi-esp32dev-1.0.3.bin` | 1.299.840 | `c3411afc703730872d2f8cd5e07a3abbec371f9c9deb1aab81603f8bc7e6f836` |

Dateien liegen in `../owlanzi-website/website/firmware/`; das lokale
`web-installer/owlanzi-tc001.bin` ist mit dem auditierten TC001-Image identisch.
Quellstand-SHA-256: `188e3e17c6b4a233c4f290269c0ac511eec4413aa8b7662106360ceb9828617f`.

## Veröffentlichung

**Erfolgreich veröffentlicht am 7. September 2026 um 13:20 UTC / 15:20 MESZ.**
Beide Manifeste auf `https://owlanzi.com/firmware/` liefern Version **1.0.3**.
Alle 14 veröffentlichten Dateien wurden sowohl im SSH-Staging als auch über
öffentliches HTTPS anhand ihrer SHA-256-Prüfsummen verifiziert. JSON-Dateien
werden mit `Cache-Control: no-cache` ausgeliefert.

Browserkontrolle in Chrome: Installationsknopf sichtbar und aktiviert,
Versionsanzeige „Firmware 1.0.3“, Downloadziel
`https://owlanzi.com/firmware/owlanzi-tc001-1.0.3.bin`.
Es wurde kein angeschlossenes Gerät geflasht.

Sicherung der ersetzten Dateien:
`/www/htdocs/w009acce/owlanzi.com/firmware/.release-history/1.0.3-20260907T132000Z/before/`.
Deployment-Beleg mit alten und neuen Prüfsummen:
`../owlanzi-website/website-tools/release/.pio/deployed-1.0.3-20260907T132000Z.json`.
Die älteren Version-1.0.2-Binärdateien bleiben auf dem Server erhalten.

Ein vorübergehender SSH-TCP-Timeout hatte den ersten Versuch vor dem Umschalten
unterbrochen. Nach erneuter Erreichbarkeit wurde aus einer neuen Sicherung
erfolgreich veröffentlicht. Der Publisher fasst Upload und Umschalten in einer
SSH-Verbindung zusammen und verwendet nur den konfigurierten Schlüssel.

Hardware-Flash und Live-Verhalten mit der realen Socke sind nicht geprüft.
Insbesondere die Aktualisierung des Cloud-Messzeitpunkts bei unveränderten
Werten, Laden und Abnehmen muss am echten Gerät noch beobachtet werden.
