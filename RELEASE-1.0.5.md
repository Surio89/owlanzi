# Release 1.0.5 — Updateprüfung

**Nachträglicher Hardwarebefund:** Die Updateprüfung startet auch mit 1.0.5
neu. Das serielle Protokoll bestätigt einen Task-Watchdog wegen ausgehungertem
IDLE0; die Stack-Vermutung war nicht die bestätigte Ursache. 1.0.6 ersetzt
diese Version. Die folgenden Angaben dokumentieren die damalige Veröffentlichung.

Veröffentlicht am **7. September 2026 um 17:13:33 UTC / 19:13:33 MESZ**
über `allinkl` nach `/www/htdocs/w009acce/owlanzi.com`.

## Befund und Korrektur

Gemeldet: Die Uhr startet bei „Check for updates“ neu; „Latest version“ bleibt
leer. Die Geräte-API wurde lesend geprüft: 1.0.4, Updatephase `idle`, keine
abgerufene Version. Es wurde kein weiterer Absturz absichtlich ausgelöst und
kein Gerät geflasht. Ohne serielles Absturzprotokoll ist die genaue Resetursache
nicht bestätigt. Der geringe verbleibende Taskstack ist eine plausible Ursache.

Das tatsächliche ESP32-Programm von 1.0.4 reservierte vor dem HTTPS-Aufruf
4480 Bytes für Netzwerk-Task, Update-Tick, Manifestabruf und Verbindungsaufbau.
Der gesamte Task hatte nur 10240 Bytes. Der Compiler hatte den Installationspfad
mit dessen Puffer in den Tick integriert; zusätzlich lag das 2-KB-JSON auf dem
Stack während TLS. Die neue Firmware trennt Prüfen und Installieren mit
`noinline`, legt beide Transferpuffer nach dem TLS-Aufbau auf den Heap und
reserviert 16384 Bytes Taskstack. Die jeweiligen Anwendungscaller benötigen
höchstens 1056 Bytes; mindestens 15328 Bytes bleiben für HTTPS und dessen
weitere Aufrufe. Dies ist eine statische Obergrenze unserer Caller, keine
Messung des gesamten TLS-Stacks auf dem Gerät.

Außerdem: korrekte Sekundenwerte für WiFiClientSecure und explizit 15 Sekunden
Handshake-Zeitlimit, Abfangen fehlender HTTP-Streams, Erhalt der zuletzt
erfolgreich geprüften Version bei weiteren Prüfungen und Fehlern, verständlicher
Text für ungeprüfte Versionen bzw. unerwarteten Neustart während der Prüfung.
Update-JSON enthält HTTP-/TLS-Fehler und den Stack-Wasserstand; der serielle
Bootlog nennt Version und Resetursache. Einstellungen und Partitionslayout
bleiben unverändert.

## Installation auf betroffenen Geräten

Die [TC001-OTA-Datei 1.0.5](https://owlanzi.com/firmware/owlanzi-tc001-1.0.5-ota.bin)
einmalig unter System → Firmware update → Manual firmware upload hochladen.
Dieser Weg schreibt nur die Anwendung und erhält NVS. Bei 1.0.4 kann der
defekte Onlineabruf die Korrektur nicht zuverlässig selbst installieren.

## Validierung

- 181 native Assertions, 11 Browsertests, 30 Website-Tests und 18 Python-Tests:
  **240 bestanden**. Netzwerktransport, Flash und NVS im nativen Test simuliert.
- Neue Fälle für reine Prüfung einer gleichen/neuen Version ohne Flash/Neustart,
  Versionsanzeige über die Status-API, fehlenden Stream, TLS-Diagnose, korrekte
  Zeitlimits, Fehler nach erfolgreicher Prüfung und Neustartanzeige im Browser.
- `tests/check_update_stack.py` prüft echte ESP32-Maschinencode-Frames und die
  Taskgröße für Release, TC001 und ESP32dev. Alle drei bestanden. Der reguläre
  Regressionrunner führt diese Prüfung künftig nach dem Build ebenfalls aus.
- Alle drei Builds erfolgreich; cppcheck warning/performance/portability der
  vier betroffenen Implementierungen ohne Befund. JavaScript-Syntax, UTF-8,
  zehn Webseiten und 385 lokale Verweise geprüft.
- Öffentliche Images ohne private Seeds; Anwendung in USB und OTA bytegleich;
  4 MB, DIO/40 MHz und unverändertes NVS-/OTA-Layout auditiert.
- 16 veröffentlichte Dateien per öffentlichem HTTPS anhand SHA-256 verifiziert.
  Beide OTA-Manifeste und Anwendungen zusätzlich per HTTP/1.0, mit Host-Header
  und `Accept-Encoding: identity` geprüft: Status 200, exakter Content-Length,
  keine Komprimierung, korrekter SHA-256.
- Öffentlicher Browser-Installer zeigt 1.0.5 und ist bereit. Betreiberkonfiguration,
  beide Impressumsseiten und `assets/site.js` vor/nach Veröffentlichung hashgleich.
- Physische Installation und tatsächlicher Updatecheck mit 1.0.5 noch ausstehend.

## Artefakte und Rückweg

| Target | Format | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| TC001 | USB | 1317360 | `48875203bee791fc2f5c5ed513c9fce693b7ed99620fa2ffac9a5ff1c432aec3` |
| TC001 | OTA | 1251824 | `d9704c3151f6941d5d90579ce80e78035e760b89213ef614f9584886ae737189` |
| ESP32dev | USB | 1317376 | `379ede4ded8d54918c658a30a16575b20622e970c5cf3014904c7f292eb9f47d` |
| ESP32dev | OTA | 1251840 | `9a9d8ac0c310ea3bdcb0fe8b5bc4387868c3110e8445dce7874524d4fc75ff13` |

Das lokale `web-installer/owlanzi-tc001.bin` und sein Manifest sind synchronisiert.
Deployment-Beleg: `../owlanzi-website/website-tools/release/.pio/deployed-1.0.5-20260907T171333Z.json`.
Sicherung: `/www/htdocs/w009acce/owlanzi.com/firmware/.release-history/1.0.5-20260907T171333Z/before`.
Frühere Binärdateien bleiben auf dem Server erhalten.
