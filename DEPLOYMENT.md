# „Update im Web ausrollen“

Das bedeutet für dieses Projekt: die aktuelle Owlanzi-Firmware bauen, prüfen
und über den bestehenden SSH-Host **allinkl** auf **owlanzi.com** veröffentlichen.
Webroot: `/www/htdocs/w009acce/owlanzi.com`. Der One-Click-Installer verwendet
`firmware/manifest.json`; der Testbuild verwendet `manifest-esp32dev.json`.
Die Geräte-WebUI verwendet `firmware/ota-tc001.json` beziehungsweise
`ota-esp32dev.json`. Seit 1.0.4 gehören USB- und OTA-Dateien zu jedem Release.
Die Aufforderung zum Ausrollen autorisiert diese Veröffentlichung. Kein Wechsel
des Hosting-Anbieters, kein Flashen angeschlossener Geräte, kein Git-Push nötig.

Lokale Projekte:

- Firmware: `C:\Users\Jannik\IdeaProjects\privat\owlanzi\owlanzi-firmware`
- Website: `C:\Users\Jannik\IdeaProjects\privat\owlanzi\owlanzi-website`
- PlatformIO/Python: `C:\Users\Jannik\.platformio\penv\Scripts`

## Vorbereiten und prüfen

1. Änderungen und Versionsstand beider Repositories prüfen. Release-Version in
   `src/owlanzi.h` und in `website-tools/release/targets.json` anheben, einschließlich
   aller versionierten USB- und OTA-Dateinamen beider Targets. Veröffentlichte Versionsdateien bleiben
   unveränderlich. Changelog aktualisieren.
2. Aus dem Firmware-Projekt `pio run -e release` und
   `python tests/run_regressions.py` ausführen. Der native Test benötigt Node.js
   und Visual Studio 2022 Build Tools. Der Runner prüft mit
   `tests/check_update_stack.py` auch die ESP32-Stackframes im Release-ELF:
   höchstens 1536 Bytes Anwendungscode vor TLS und mindestens 16 KB Netzwerk-
   Taskstack. Für beide Website-ELFs die Prüfung mit `--elf` und `--source`
   wiederholen. Keine privaten Seeds in öffentliche Builds.
   Die Prüfung verlangt außerdem `tskIDLE_PRIORITY` für den Netzwerk-Task auf
   Core 0: rechenintensive TLS-Aufrufe dürfen IDLE0 nicht vom Watchdog abhalten.
   Ein verbundener, ausdrücklich zum Testen freigegebener TC001 kann zusätzlich
   mit `tests/hardware_update_check.py --base <Geräte-URL> --current <Version>
   --latest <veröffentlichte-Version> --count 2 --output <lokaler-Prüfbeleg>`
   geprüft werden. Der Test installiert nichts; er lädt Metadaten und prüft,
   dass die Betriebszeit währenddessen nicht zurückgesetzt wird.
3. Aus dem Website-Projekt:

   ```powershell
   ./website-tools/build-firmware.ps1 -FirmwareSourceProject ../owlanzi-firmware
   node website-tools/sync-display-data.mjs
   node website-tools/pages.mjs
   node --experimental-vm-modules --test website-tools/site.test.mjs website-tools/simulator.test.mjs
   python -m unittest discover -s website-tools -p 'test_*.py' -v
   node website-tools/verify.mjs
   python website-tools/deploy-firmware.py
   ```

   `python`/`pio` ggf. durch die absoluten Pfade oben ersetzen. Der Release-Build
   übernimmt nur `.cpp`/`.h` ohne `secrets_local.h`, baut TC001 und ESP32dev mit
   `NO_LOCAL_SECRETS`, prüft beide 4-MB-Images, OTA-Partitionen, Checksummen,
   Quellstand sowie das Fehlen privater Seed-Werte. Nicht blind aus dem
   Entwicklungs-Environment `ulanzi` veröffentlichen. Der zusammengeführte
   USB-Build ist kein Anwendungsabbild für das lokale OTA-Uploadfeld.

   Je Target entstehen fünf Dateien: zusammengeführtes USB-Image, reines
   `owlanzi-<target>-<version>-ota.bin`, USB-Manifest, OTA-Manifest und Release-
   Prüfbeleg. Das OTA-Manifest enthält Version, Target, `ota-app`, Schema 1,
   Layout `owlanzi-4m-v1`, Dateiname, exakte Größe und SHA-256. Es wird aus
   demselben geprüften Anwendungsabbild wie das USB-Image erzeugt.

   Das Layout bleibt für Updates unverändert: NVS bei `0x9000`/`0x5000`,
   OTA-Daten bei `0xe000`/`0x2000`, App-Slots bei `0x10000` und `0x1f0000`
   mit je `0x1e0000` Bytes. Änderungen an Layout oder Konfigurationsschema
   benötigen eine ausdrücklich geprüfte Migration. Normale Updates schreiben
   nur den freien App-Slot und die Bootauswahl, nicht die persönlichen NVS-Daten.

## Veröffentlichen

```powershell
python website-tools/deploy-firmware.py --publish
```

Wenn auch das Ko-fi-Erfolgs-Popup geändert wurde, beim lesenden Plan und bei der
Veröffentlichung zusätzlich `--with-support-popup` angeben. Das übernimmt nur
das geprüfte Popup-Template in beide bestehenden Live-Startseiten sowie
`assets/site.js` und das angepasste Installer-Dialogmodul. Andere Live-Texte und
die zentrale Betreiber-/Linkkonfiguration bleiben erhalten.

Der Publisher nutzt `ssh allinkl`, lädt in ein neues Staging-Verzeichnis,
prüft serverseitig SHA-256 und unveränderte Bestandsdateien, sichert ersetzte
Dateien und ersetzt USB- und OTA-Manifeste zuletzt atomar. Frühere Binärdateien bleiben.
Bereits vorhandene OTA-Manifeste werden ebenfalls für Rollbacks gesichert.
Er aktualisiert nur Firmware, die zwei Display-Vorschau-Module und Versions-
und Dateiverweise in vorhandenen HTML-Seiten. Remote-Texte, Betreiberangaben,
`assets/config.js`, `.htaccess` und andere Assets werden erhalten.

Anschließend prüft er jede veröffentlichte Datei über HTTPS, einschließlich
SHA-256 der tatsächlich öffentlich ausgelieferten Binärdateien und
`Cache-Control: no-cache` der Metadaten. Installerversion auf
`https://owlanzi.com/index.de.html#installieren` zusätzlich im Browser prüfen,
ohne einen Flashvorgang zu starten. Hardwaretests nur als durchgeführt melden,
wenn sie tatsächlich erfolgt sind.

Die vorhandene Statistik lässt ausgehende HTML-Links unverändert sichtbar.
Der HTTPS-Prüfer ruft mit `no_stats=1` und DNT ab und berücksichtigt ausschließlich
die Weitergabe von `no_stats=1` an interne Links durch den vorhandenen PHP-Handler;
alle übrigen HTML-Bytes und sämtliche Firmware-Dateien müssen exakt übereinstimmen.
Die Prüfung erzeugt keine Statistikzählungen und liest keine private PHP-/
Datenbankkonfiguration. Serverdateien und Sicherungen bleiben im Rohformat.

Die OTA-Metadaten und Images müssen per HTTPS ohne Weiterleitung, mit exaktem
`Content-Length` und ohne Komprimierung bei `Accept-Encoding: identity`
abrufbar sein. Vor dem Freigeben des nächsten Boots prüft das Gerät die
SHA-256 des vollständigen Downloads; Einstellungen werden dabei nicht gelöscht.

## Geräte mit älterer Firmware aktualisieren

1.0.3 hat noch keinen Online-Updateknopf. Einmalig die zum Gerät passende
aktuelle Datei mit Endung `-ota.bin` aus `https://owlanzi.com/firmware/` im
vorhandenen Firmware-Upload der Geräte-WebUI installieren. Dieser Weg erhält
NVS. Ab 1.0.4: **System → Firmware aktualisieren → Neuestes Update installieren**.
Falls 1.0.4 oder 1.0.5 bei der Onlineprüfung neu startet, einmalig die 1.0.6-
`-ota.bin` über das manuelle Uploadfeld installieren. Die Korrektur kann auf
betroffenen Geräten nicht zuverlässig über deren bisherigen Onlineknopf kommen.
Der USB-Installer schreibt auch Boot-/Partitionsdaten und eignet sich nicht
als Verfahren mit zugesichertem Einstellungserhalt.

Beleg mit Prüfsummen und Sicherungspfad:
`website-tools/release/.pio/deployed-<Version>-<UTC>.json`.
Sicherung auf dem Host: `firmware/.release-history/<Version>-<UTC>/before/`.
Dieser Ordner ist per `.htaccess` für HTTP gesperrt.

## Fehler und Rückweg

Bei einem Build-, Audit-, Konkurrenz- oder Integritätsfehler keine weiteren
Dateien veröffentlichen. Ursache prüfen; keine unbeschränkte Wiederholung.
Bei fehlgeschlagener öffentlicher Prüfung nach dem Umschalten zuerst den
Deployment-Beleg und die ausgelieferten Dateien vergleichen.

Für einen angeforderten Rollback die im Beleg genannten Dateien aus `before/`
auf dem gleichen Host erst auf temporäre Geschwisterpfade kopieren, dann mit
`mv` atomar ersetzen. Zuerst die alten Manifeste wiederherstellen, danach die
übrigen gesicherten Dateien; alte Binärdateien sind weiterhin vorhanden. Die
öffentlichen Prüfsummen gegen `previous_sha256` prüfen. Keine rekursiven Lösch-
oder Mirror-Deployments im Webroot und keine alten Versionen ungefragt entfernen.

Das lokale `web-installer/owlanzi-tc001.bin` bei einer Veröffentlichung mit dem
auditierten TC001-Build synchronisieren; dortiger Manifestpfad bleibt unversioniert,
die Versionsangabe muss aber dem veröffentlichten Build entsprechen.
