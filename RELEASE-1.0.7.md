# Release 1.0.7 — Desktop-WebUI

Am 7. September 2026 um 19:59:27 UTC über `allinkl` auf owlanzi.com
veröffentlicht; abschließende öffentliche Prüfung erfolgreich.

## Änderungen

Die Geräte-WebUI erhält eine feste linke Desktop-Navigation, einen mittigen
Inhaltsbereich und eine Scrollleiste am rechten Fensterrand. Auch auf ultrabreiten
Monitoren bleiben die Einstellungsbereiche übersichtlich untereinander und
dauerhaft offen. Mobile Bedienung, Live-Vorschau und deutsche/englische Texte
bleiben verfügbar. Speichern ist erst nach dem Laden der Gerätekonfiguration
möglich; ungültige Felder werden im passenden Tab angezeigt.

Gegenüber dem vorherigen Release-Quellsnapshot wurden nur `webui.cpp` und die
Versionsnummer geändert. Konfigurationsschema, NVS und OTA-Layout bleiben gleich.

## Prüfung

- Release-, TC001- und ESP32dev-Build erfolgreich, ohne private Seeds.
  USB- und OTA-Dateien stammen jeweils aus demselben auditierten 4-MB-Build.
- 182 native Assertions, 12 JavaScript-Tests, 32 Website-Tests und 30
  Python-Tests bestanden. Die feste zukünftige Versionsnummer in den nativen
  Update-Testdaten wird nun aus der aktuellen Version abgeleitet.
- Alle drei ELF-Dateien bestehen die Stackprüfung: höchstens 1056 Bytes
  Anwendungscaller, 16384 Bytes Netzwerk-Taskstack und Idle-Priorität.
- Die WebUI wurde vor der Veröffentlichung bei 3440 × 1440 und 390 × 844
  geprüft: acht Browser-Prüfgruppen je Größe, beide Sprachen und alle vier Tabs.
  Der angeschlossene ESP32 erhielt diese UI bereits als Entwicklungsbuild mit
  Versionsangabe 1.0.6; Einstellungen blieben erhalten. Die ultrabreite Ansicht
  wurde auch direkt vom Gerät geprüft.
- Nach Veröffentlichung erkannte derselbe ESP32 1.0.7 zweimal als verfügbares
  Update: 17,91 und 12,66 Sekunden; Betriebszeit 980 → 998 → 1010 Sekunden,
  HTTP 200, TLS-Fehler 0. Dabei wurde nichts installiert. Das finale 1.0.7-Image
  wurde im Rahmen dieser Veröffentlichung nicht erneut auf Hardware geflasht.
- Alle 16 veröffentlichten Dateien über HTTPS geprüft. Beide OTA-Ziele liefern
  unter HTTP/1.0 mit Host-Header unveränderte Bytes, exaktes Content-Length,
  keine Weiterleitung und keine Komprimierung bei Accept-Encoding: identity.
- Der öffentliche Browser-Installer zeigt Firmware 1.0.7; beide USB-Manifeste
  referenzieren die richtigen Dateien. Der lokale `web-installer` ist synchron.
- Zehn geschützte Bestandsdateien einschließlich Betreiberkonfiguration,
  rechtlicher Seiten und Statistik-Konfiguration sind unverändert.

Die erste öffentliche HTML-Prüfung meldete nach erfolgreichem Umschalten eine
Abweichung durch die bestehende Statistik-Linkumschreibung. Der Prüfer wurde
dafür gezielt erweitert; drei Regressionstests sichern erlaubte Linkänderungen,
die Erkennung fremder HTML-Änderungen und bytegenaue Firmwareprüfungen ab.
Anschließende SSH-Timeouts wurden für die lesende Bestandsprüfung über den
freigegebenen FTPS-Zugang mit TLS-Prüfung überbrückt. Es war kein erneuter Upload
und kein Rollback nötig. Die vollständige Prüfung ist erfolgreich.

## Belege

Im Website-Projekt:

- `website-tools/release/.pio/deployed-1.0.7-20260907T195927Z.json`
- `website-tools/release/.pio/public-verified-1.0.7.json`
- `website-tools/release/.pio/protected-before-1.0.7.json`

Im Firmware-Projekt:
`.pio/tests/hardware-1.0.7-after-publish.json`.

Serversicherung:
`/www/htdocs/w009acce/owlanzi.com/firmware/.release-history/1.0.7-20260907T195927Z/before/`.
