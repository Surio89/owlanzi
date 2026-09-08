# Release 1.0.6 — Wiederherstellung und bestätigter Watchdog-Fix

Status: auf dem TC001 installiert und geprüft; am 7. September 2026 um
18:55:43 UTC (20:55:43 MESZ) auf owlanzi.com veröffentlicht.

## Veröffentlichung

Die vorübergehenden SSH-Timeouts sind beendet. Nach erfolgreichem lesendem Plan
wurde 1.0.6 über `allinkl` in `/www/htdocs/w009acce/owlanzi.com` veröffentlicht.
Der Publisher bestätigte serverseitige Prüfsummen und anschließend alle
16 veröffentlichten Dateien über HTTPS. USB- und OTA-Manifeste für TC001 und
ESP32dev liefern nun 1.0.6. Beide OTA-Metadateien und Anwendungsabbilder wurden
zusätzlich mit HTTP/1.0, `Accept-Encoding: identity`, exaktem Content-Length
und vollständigem Bytevergleich geprüft. Betreiberkonfiguration, beide
Impressumsseiten und `assets/site.js` sind gegenüber der unmittelbar zuvor
gelesenen öffentlichen Fassung unverändert.

Der öffentliche Browser-Installer zeigt Firmware 1.0.6. Der lokale
`web-installer` wurde mit dem auditierten TC001-USB-Image und der passenden
Manifestversion synchronisiert; die SHA-256 stimmt überein.

Zwei weitere Versionsprüfungen auf der echten Uhr bestanden in 12,59 und
16,67 Sekunden. Betriebszeit 675 → 688 → 705 Sekunden, kein Neustart,
HTTP 200, TLS-Fehler 0. Sowohl Geräte-API als auch die bereits geöffnete
Systemseite zeigen installiert 1.0.6 und neueste Version 1.0.6 an.
Es war keine weitere Installation auf der Uhr notwendig.

Belege im Website-Projekt:

- `website-tools/release/.pio/deployed-1.0.6-20260907T185543Z.json`
- `website-tools/release/.pio/public-ota-1.0.6.json`
- `website-tools/release/.pio/protected-before-1.0.6.json`

Geräteprüfung im Firmware-Projekt:
`.pio/recovery/20260907-com6/checks-1.0.6-final-after-publish.json`.

Serversicherung:
`/www/htdocs/w009acce/owlanzi.com/firmware/.release-history/1.0.6-20260907T185543Z/before/`.

## Rettung des Geräts

Windows erkannte den CH340 weiterhin als COM6. `esptool flash_id` identifizierte
einen ESP32-D0WD Revision 1.1 und 4 MB Flash. Vor Schreibversuchen wurde der
gesamte 4-MB-Speicher lokal unter `.pio/recovery/20260907-com6/before.bin` gesichert.
Diese ignorierte, private Sicherung enthält persönliche Einstellungen und darf
nicht veröffentlicht werden. SHA-256 der Sicherung:
`0d2f280bde5f68ee493ba280eda13897477023d5377bbaf558720d528036a29a`.

Die Sicherung zeigte: Bootloader, Partitionstabelle und OTA-Startdaten entsprachen
dem geprüften Release; App0 war bytegenau die öffentliche 1.0.4-Anwendung. App1
war leer, NVS enthielt die Owlanzi-Konfiguration. Der im Browser hängende
Löschvorgang hatte die ursprüngliche Firmware daher nicht entfernt.

USB-Schreibversuche brachen beim Flashzugriff ab, zunächst mit Stub, danach
auch über ROM bei 115200 Baud. Der anschließende serielle Reset startete die
vorhandene Anwendung und stellte WLAN unter `192.168.100.26` wieder her.
Die Wiederherstellung/Installation erfolgte danach erfolgreich über den
vorhandenen manuellen OTA-Endpunkt. Die über die Konfigurations-API ausgelesenen
Einstellungen waren nach dem Update unverändert; WLAN und Owlet-Anmeldung
funktionierten. Nach Entfernen des USB-Kabels wurde ausschließlich über WLAN
weitergearbeitet. Die genaue Ursache der sporadischen USB-Flashfehler ist offen.

## Bestätigte Ursache der Neustarts

Die Updateprüfung ließ auch 1.0.5 neu starten. Das echte serielle Protokoll
meldete `task_wdt`, IDLE0 ohne rechtzeitige Watchdog-Bedienung, während `net`
auf Core 0 lief; Resetursache 6. Der ursprüngliche Verdacht auf einen
Stacküberlauf war nicht die bestätigte Ursache. Der Status in RELEASE-1.0.5.md
und CHANGELOG.md wurde entsprechend berichtigt.

Der Netzwerk-Task läuft jetzt mit `tskIDLE_PRIORITY` auf Core 0. Dadurch kann
das RTOS während langer TLS-Berechnungen auch IDLE0 ausführen. Der Watchdog
bleibt aktiv und wird regulär vom Idle-Task bedient. Zusätzlich werden positive
WiFiClientSecure-Rückgaben (Socketdeskriptoren) nicht als TLS-Fehler ausgegeben.
Stack- und Timeoutverbesserungen aus 1.0.5 bleiben enthalten.

## Prüfungen

- 182 native Assertions, 11 Browser-, 30 Website- und 18 Python-Tests:
  **241 automatisierte Prüfungen bestanden**.
- Release-, TC001- und ESP32dev-Build erfolgreich; NO_LOCAL_SECRETS und Images
  für 4 MB DIO/40 MHz auditiert. NVS und beide OTA-Partitionen unverändert.
- Alle drei ELF-Dateien bestehen die Stackprüfung: höchstens 1056 Bytes
  Anwendungscaller, 16384 Bytes Taskstack. Die Prüfung sichert auch die
  Watchdog-verträgliche Task-Priorität ab.
- cppcheck für main.cpp und online_update.cpp ohne Befund. Browser-Syntax,
  UTF-8 und zehn Webseiten mit 385 lokalen Verweisen geprüft.
- Finales 1.0.6-Image auf dem echten TC001 manuell per OTA installiert.
  Zwei HTTPS-Versionsprüfungen vor Veröffentlichung bestanden in 30,23 und
  21,02 Sekunden. Betriebszeit 27 → 57 → 79 Sekunden, kein Neustart,
  HTTP 200, TLS-Fehler 0, kleinster freier Taskstack 10052 Bytes. Angezeigte
  neueste Version zu diesem Zeitpunkt korrekt 1.0.5 vom öffentlichen Server.
- Reproduzierbarer Gerätetest: `tests/hardware_update_check.py`.
  Belege: `.pio/recovery/20260907-com6/checks-1.0.6-final-before-publish.json`.
  Der frühere Testbuild lief bereits mehr als 38 Minuten und war bei Owlet
  angemeldet. Der finale Build enthält zusätzlich korrigierte TLS-Diagnosen.
- Ein kompletter automatischer Online-Download mit anschließendem Flashen
  wurde noch nicht physisch getestet; dafür besteht simulierte Abdeckung.

## Geprüfte Release-Dateien

| Target | Format | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| TC001 | USB | 1317360 | `32443e6a659536f022eb8a4646402cb36ce4783988f5d6d4d444e56e10142197` |
| TC001 | OTA | 1251824 | `4798bc04e23024eb5d07b2204785b2f11300652d69a2badf601fec38e2f6544d` |
| ESP32dev | USB | 1317376 | `3511389a5d12207d85b024ba357c0332b60efed6fcfbbe43447d08b284f356fd` |
| ESP32dev | OTA | 1251840 | `8c0b983d99efc4a5a79f1588c17be26fed57cda57c82ebf4e1cfa2daa6281d2e` |

Quellstand-SHA-256:
`2e8cabd1e6d32ccf790139857ad85abc4307ca570ada8074f21462bd97748dbe`.
