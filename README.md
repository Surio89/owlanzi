# owlanzi

Eigene Firmware für die **Ulanzi TC001**, die den Owlet Smart Sock direkt aus
der Owlet-Cloud abfragt und Puls, Sauerstoffsättigung, Schlafzustand und
Akkustand auf der 8×32-Matrix anzeigt. Kein Home Assistant, kein Broker, kein
Server dazwischen — die Uhr holt sich die Werte selbst und wird über eine
Weboberfläche im eigenen WLAN eingerichtet.

> **Die Uhr ersetzt die Basisstation nicht.** Sie ist eine zusätzliche
> Anzeige, weiter nichts. Sie ist kein Medizinprodukt, sie ist nicht geprüft,
> und sie kann ausfallen — WLAN weg, Cloud weg, Firmware mit Fehler. Die
> Alarme der Basisstation bleiben die, auf die du dich verlässt.

<details>
<summary><b>In English</b></summary>

Custom firmware for the Ulanzi TC001 pixel clock. It polls the Owlet Smart
Sock cloud directly and shows heart rate, oxygen saturation, sleep state and
sock battery on the 8×32 matrix — no Home Assistant, no broker, no bridge.
Setup runs through a web interface on your own network; the UI speaks English
and German.

**This is an additional display, not a replacement for the Owlet base
station.** It is not a medical device and it is not certified. Keep relying on
the base station's alarms.

Source comments and this README are in German; the web interface is
bilingual.
</details>

---

## Was du brauchst

- eine **Ulanzi TC001** (ESP32-WROOM-32D, 8 MB Flash)
- ein **Owlet-Konto** mit einem angelernten Smart Sock
- ein 2,4-GHz-WLAN
- ein USB-C-Kabel zum ersten Aufspielen

## Aufspielen

Im Ordner `ulanzi-firmware/web-installer/` liegt eine fertige Seite für
[ESP Web Tools](https://esphome.github.io/esp-web-tools/): `index.html`,
`manifest.json` und das zusammengesetzte Abbild. Über einen Browser mit
Web-Serial (Chrome, Edge) lässt sich die Uhr damit ohne Toolchain flashen.

Selbst bauen geht mit [PlatformIO](https://platformio.org/):

```
pio run -e ulanzi -t upload
```

**Zurück zu AWTRIX** führt nicht über ein Update: das ersetzt nur das
Programm, nicht die Partitionstabelle. Dafür den AWTRIX-Web-Flasher über USB
benutzen.

## Einrichten

Beim ersten Start öffnet die Uhr einen offenen Hotspot **owlanzi**. Verbindest
du dich damit, geht die Einrichtungsseite von selbst auf (sonst
`192.168.4.1`). Zwei Angaben genügen: WLAN und Owlet-Konto. Danach startet die
Uhr neu und zeigt 15 Sekunden lang ihre IP-Adresse — unter der erreichst du
die vollständige Oberfläche.

Dort lassen sich unter anderem einstellen:

- **Farben** für jeden Schirm, mit einer Vorschau, die sich beim Verstellen
  sofort mitfärbt
- **Helligkeit**, getrennt für Normalbetrieb, Tageslicht, Alarm und Vorschau,
  gesteuert über den Lichtsensor
- **eigene Alarme** auf Sauerstoff und Puls, jeweils mit Grenzwert und
  Mindestdauer
- **Abrufabstand** zur Owlet-Cloud (5, 10 oder 15 Sekunden)
- **Sprache** der Oberfläche und der Texte auf der Matrix

Die Zugangsdaten bleiben auf dem Gerät. Die Verbindung zur Owlet-Cloud prüft
Zertifikate.

## Wie die Anmeldung funktioniert

Die Owlet-Cloud hängt an Ayla Networks; die Anmeldung ist eine Kette aus vier
Schritten:

1. Firebase: E-Mail + Passwort → `idToken`
2. Owlet-SSO: `idToken` → `mini_token`
3. Ayla: `mini_token` + App-Secret → `access_token`
4. Ayla: `APP_ACTIVE=1` setzen, dann `properties.json` lesen

Schritt 4 ist nicht optional: ohne `APP_ACTIVE` liefert die Cloud
eingefrorene Werte, sie aktualisiert nur, solange eine App zuhört.

Endpunkte und Feldnamen stammen aus den quelloffenen Python-Implementierungen
[pyowletapi](https://github.com/ryanbdclark/pyowletapi) und
[owlet_monitor](https://github.com/mbevand/owlet_monitor).

## Aufbau

```
ulanzi-firmware/
  src/            Firmware (main, Anzeige, Owlet-Anbindung, Weboberfläche)
  tools/          PowerShell-Werkzeuge: bauen, aufspielen, prüfen, ablichten
  web-installer/  fertiges Abbild plus ESP-Web-Tools-Seite
  assets/         Logo und Vorschaubild
  experiments/    der erste Versuch: nur Anmeldung und Abruf, ohne Anzeige
```

Drei PlatformIO-Umgebungen:

| Umgebung   | wofür                                                        |
|------------|--------------------------------------------------------------|
| `esp32dev` | nacktes Entwicklerboard, 4 MB, ohne Matrix                    |
| `ulanzi`   | die TC001 selbst, 8 MB, mit OTA-Partitionen                   |
| `release`  | wie `ulanzi`, aber ohne die lokale `secrets_local.h`          |

### Zugangsdaten beim Entwickeln

`src/secrets_local.h` (Vorlage: `secrets_local.h.example`) befüllt den NVS
einmalig, damit man beim Testen nicht jedes Mal durch den Hotspot muss. Die
Datei steht in `.gitignore` — **ihre Zeichenketten landen aber beim
Übersetzen im Abbild.** Eine so gebaute `.bin` darf nicht weitergegeben
werden.

Deshalb: was veröffentlicht wird, baut `tools/make-installer.ps1` aus der
Umgebung `release` und durchsucht das fertige Abbild anschließend noch einmal
nach den Zeichenketten. Findet es etwas, löscht es die Datei und bricht ab.

## Was noch offen ist

Ehrlicher Stand, keine Wunschliste:

- **Schlafzustand.** Die Zahlenwerte des Felds `ss` sind nicht dokumentiert.
  Bekannt zugeordnet sind wach, leichter und tiefer Schlaf; alles andere
  fällt bewusst auf den grauen Balken, statt eine Tiefe vorzutäuschen.
- **Lichtschwelle.** Der Vorgabewert stammt aus AWTRIX und ist auf der TC001
  noch nicht nachgemessen.
- **Frischeprüfung.** Dass die Anzeige wirklich auf „warten" umschaltet, wenn
  die Socke von der Ladeschale kommt, ist noch nicht mit einer echten Socke
  durchgespielt.

## Rechtliches

Owlet und Ulanzi sind Marken der jeweiligen Eigentümer. Dieses Projekt steht
in keiner Verbindung zu beiden. Es benutzt eine nicht öffentlich
dokumentierte Schnittstelle, die sich jederzeit ändern kann.
