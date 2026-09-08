# Release 1.0.4 — Prüfbeleg

Veröffentlicht am **7. September 2026 um 15:19 UTC / 17:19 MESZ** über
`ssh allinkl` nach `/www/htdocs/w009acce/owlanzi.com`.

## Verhalten

Die Geräte-WebUI bietet unter **System → Firmware aktualisieren** eine
Versionsprüfung und **Neuestes Update installieren**. Das Gerät lädt die
neueste kompatible Version von owlanzi.com und startet nach erfolgreicher
Prüfung neu. WLAN, Owlet-Konto, Anzeige- und Alarmeinstellungen bleiben im
bestehenden NVS. Die neue Firmware schreibt ausschließlich den freien
Anwendungs-Slot und aktiviert ihn erst nach vollständiger SHA-256-Prüfung.

HTTP-Fehler, unterbrochene Downloads, Prüfsummenfehler, falsche Hardware/
Speicheraufteilung und konkurrierende Uploads werden abgefangen. Ein aktiver
kritischer Alarm verhindert den Start. Während der Installation pausieren
Cloud-Abrufe; der Netzwerk-Task führt den Download mit begrenztem Speicher
und Zeitlimit aus. Es gibt keinen unbeaufsichtigten automatischen Updateversuch.

Die Firmwarekorrekturen aus 1.0.3 sind enthalten. Zusätzlich wurden falsch
codierte deutsche WebUI-Texte repariert.

## Bestehende Geräte

1.0.3 hat den neuen Knopf noch nicht. Einmalig
[owlanzi-tc001-1.0.4-ota.bin](https://owlanzi.com/firmware/owlanzi-tc001-1.0.4-ota.bin)
im bisherigen Firmware-Upload der Geräte-WebUI installieren. Dieser Weg
erhält die Einstellungen. Danach erfolgen weitere Updates über den neuen
Knopf. Das kombinierte USB-Image ist keine OTA-Upload-Datei.

## Validierung

- 171 native C++-Prüffälle gegen Produktionslogik, einschließlich kompletter
  Download-Verarbeitung mit kleinen Transportblöcken, echter SHA-256 im Host-
  Adapter, Schreib-/Abbruch-/Neustartkontrolle, unveränderter Konfiguration
  und unveränderter Anzahl von NVS-Schreibzugriffen.
- 8 Browsertests: Fortschritt, gesperrte Uploads, Fehleranzeige, genau ein
  Installationsrequest pro Klick, Wiederverbindung und Ausfallanzeige.
  Beide eingebetteten Browserprogramme zusätzlich auf Syntax und die
  Quelldatei auf erneut beschädigte UTF-8-Codierung geprüft.
- 30 Website-Tests, 8 Image-Audittests und 10 Deploymenttests. Die echten
  Deployment-Shellbefehle liefen zusätzlich in isolierten lokalen Testordnern,
  einschließlich erster und späterer OTA-Veröffentlichung, Sicherungen,
  konkurrierender Änderung und unveränderlicher Versionsdateien.
- Release-, TC001- und ESP32dev-Build erfolgreich: RAM 53.640/327.680 Bytes;
  Programm 1.243.625/1.966.080 Bytes je OTA-Slot (63,3 %).
- cppcheck warning/performance/portability für alle sieben Implementierungs-
  dateien ohne Befund. 10 HTML-Seiten und 387 lokale Verweise geprüft.
- Beide Targets: 4 MB, DIO/40 MHz, zwei unveränderte OTA-Slots und NVS-Bereich;
  Image-/Partitionsprüfsummen korrekt, Anwendung in USB und OTA bytegleich.
  `NO_LOCAL_SECRETS` aktiv, drei private Seed-Werte geprüft und nicht enthalten.
- Lokale Browserkontrolle der echten Updatekarte mit simulierten Antworten:
  deutsche Texte, manuelle Upload-Auswahl, Versionsanzeige und Updateprüfung.
  Keine echte Geräteverbindung in diesem Test.
- Persönlicher Skill `owlanzi-web-release` um OTA-Veröffentlichung ergänzt;
  Skill-Validator erfolgreich. Details in `DEPLOYMENT.md`.

## Dateien und Prüfsummen

| Target / Format | Datei | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| TC001 USB | `owlanzi-tc001-1.0.4.bin` | 1.315.744 | `107bf90c1daba442b176ae17faa916d31fb58c55e61735a39170bd0aaa2ad274` |
| TC001 OTA | `owlanzi-tc001-1.0.4-ota.bin` | 1.250.208 | `04533ae3ebdcb271c0f5644430ef24116acd15f5c18fb794673bdc1c4718d8ab` |
| ESP32dev USB | `owlanzi-esp32dev-1.0.4.bin` | 1.315.744 | `b11e57011051fc594682137c797da7f6b8d978f917eecfcdabe4b28ee18cd64e` |
| ESP32dev OTA | `owlanzi-esp32dev-1.0.4-ota.bin` | 1.250.208 | `e7f419d7ce041bd7dcab0f11807285ed9c1e85a6a9b36ab79db325e6beeabd69` |

Quellstand-SHA-256:
`b780a345782608ef5b284f46bc8ffeb5a67504dd305e2ffba37d879655bc584c`.
Das lokale `web-installer/owlanzi-tc001.bin` ist mit dem veröffentlichten
TC001-USB-Image synchronisiert; sein Manifest nennt ebenfalls 1.0.4.

## Öffentliche Prüfung und Sicherung

Alle **16 veröffentlichten Dateien** wurden im SSH-Staging und danach per
öffentlichem HTTPS vollständig anhand SHA-256 geprüft. USB- und OTA-Manifeste
liefern 1.0.4 mit `Cache-Control: no-cache`. OTA-Metadaten und beide Anwendungen
wurden zusätzlich mit HTTP/1.0 und `Accept-Encoding: identity` abgerufen:
HTTP 200, exakter Content-Length, keine Komprimierung, identische Bytes.

Der öffentliche Installer in Chrome zeigt **Firmware 1.0.4**, einen aktivierten
Installationsknopf und den passenden USB-Download. Es wurde kein Gerät geflasht.

Deployment-Beleg:
`../owlanzi-website/website-tools/release/.pio/deployed-1.0.4-20260907T151914Z.json`.
Sicherung der ersetzten Dateien:
`/www/htdocs/w009acce/owlanzi.com/firmware/.release-history/1.0.4-20260907T151914Z/before/`.
HTTP-Zugriff auf die Sicherung wurde mit Status 403 abgewiesen. Ältere
Binärdateien bleiben erhalten; Betreibertexte und sonstige Assets bleiben bestehen.

## Grenzen der Prüfung

Flash, NVS, Uhrzeit und Transport sind in nativen Tests simuliert. Eine reale
TC001-Installation, Stromunterbrechung und Wiederaufnahme der Owlet-Verbindung
nach einem echten Neustart sind noch nicht geprüft. Die Behandlung defekter
Downloads ist getestet; ein automatischer Rückfall nach einem Laufzeitfehler
der bereits gestarteten neuen Firmware ist nicht implementiert. Gleich alte
oder ältere Online-Releases werden nicht automatisch installiert.
