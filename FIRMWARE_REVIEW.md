**Firmware-Review · 7. September 2026**

**Behebungsstand 1.0.3:** Die 13 unten dokumentierten Befunde sind im aktuellen
Quellcode korrigiert. Auch die ergänzenden Punkte zu Geräteauswahl, Timerüberlauf,
WLAN-Status, Browser-Ausfallanzeige und blockierenden Web-/Tonabläufen wurden
bearbeitet. `tests/run_regressions.py` prüft 98 Fälle erfolgreich gegen die
Produktionsfunktionen mit simulierten Hardware- und Transportzugriffen.
Details stehen in [CHANGELOG.md](CHANGELOG.md). Der folgende Bericht bewahrt
die ursprünglichen Befunde und Zeilenangaben des geprüften alten Standes;
er beschreibt nicht den aktuellen Fehlerstand. Ein Hardwaretest steht aus.

Geprüfter Stand: `fca313ff71ebf14e6579e0dfee3a4c1b0d6407a1`. Umfang: alle fünf Implementierungsdateien und vier gemeinsamen Header in `src` (ohne den Inhalt privater Zugangsdaten offenzulegen), Cloud-Anmeldung und Parsing, Zustandsübergänge, Alarme, Matrix und Ton, Weboberfläche einschließlich Setup und OTA, NVS sowie Build- und Installer-Artefakte. Der Firmware-Quellcode wurde für diese Review nicht verändert; es wurde nichts geflasht oder veröffentlicht.

**Der gemeldete Schlafstatusfehler ist im Code reproduziert.** Der Parser übernimmt `REAL_TIME_VITALS.ss` korrekt. Erst die Interpretation ist falsch: `8` wird sowohl auf der Matrix als auch in der Weboberfläche als „wach“ behandelt. Die Home-Assistant-Integration des Autors von `pyowletapi` verwendet folgende Zuordnung:

| Rohwert `ss` | Referenzimplementierung | Owlanzi aktuell |
| --- | --- | --- |
| 0 | unbekannt | unbekannt |
| 1 | wach | wach |
| 8 | leichter Schlaf | **wach / langer gelber Balken** |
| 15 | Tiefschlaf | **unbekannt / grauer Balken** |

Quelle: [SLEEP_STATES in ryanbdclark/owlet](https://github.com/ryanbdclark/owlet/blob/main/custom_components/owlet/const.py), abgerufen am 6. September 2026, Datei-Blob `041727da5cad4c205d4b607663499456981ff7db`. Das ist eine überprüfte Integrationsimplementierung, keine offizielle Protokollspezifikation von Owlet. Der konkrete Live-Rohwert deiner Socke konnte nicht zusätzlich gelesen werden: Die in den Projektwerkzeugen hinterlegte Geräteadresse antwortete auf den rein lesenden Statusabruf nicht innerhalb des Zeitlimits. Bei `ss=8` erklärt der nachgewiesene Fehler deine Beobachtung vollständig.

**Befunde**

P1 bedeutet hohe Priorität wegen unterdrückter Alarme, falscher Datenfreigabe, Absturzrisiko oder ungeschütztem Geräteeingriff. P2 bezeichnet weitere funktionale Fehler. Die Reihenfolge beginnt mit dem gemeldeten Problem und anschließend den dringenden Befunden. „Reproduziert“ bezieht sich auf den lokalen C++-Test mit simulierten Hardware- und Netzwerkzugriffen, nicht auf einen Test am Kind oder an der realen Socke.

**1 · P2 · Schlafcodes zentral und korrekt zuordnen — reproduziert.**

In [display.cpp:99](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/display.cpp:99) sowie [webui.cpp:1082](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/webui.cpp:1082) werden `1/8`, `2/9` und `3/10` jeweils gleichgesetzt. Für die zusätzlichen Codes `2/3/9/10` ist keine Zuordnung in der geprüften Referenz belegt. Die künstlichen Matrix-Testwerte sind ebenfalls `8/9/10`; die Browser-Farbvorschau verwendet ihre eigene Skala `1/2/3`. Deshalb kann die Vorschau plausibel aussehen, obwohl echte Cloudwerte falsch dargestellt werden. Korrektur: einen gemeinsamen Decoder für `0/1/8/15` verwenden, unbekannte Werte unbekannt lassen und Matrix, Statusnamen und Testdaten daraus ableiten. Die vier Prüfungen für englischen/deutschen Namen, Tiefschlaf und blaue Matrixdarstellung schlagen fehl; die reine Übernahme von `ss=8` besteht.

**2 · P1 · Alarmzustand wird zwischen beiden Kernen ohne gemeinsamen Schreibschutz geändert — statisch belegt.**

[config.cpp:163](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/config.cpp:163) ersetzt `gAlarmText` und `gAlarmCritical` ohne Mutex. `alarmRecompute()` läuft aus der Netzwerktask auf Core 0 und aus dem Konfigurationshandler auf Core 1. Gleichzeitig kopieren Matrix und Webstatus den `String` unter `stateLock()`. Diese Lesesperre schützt nicht vor einem Schreiber, der dieselbe Sperre gar nicht nimmt: Speicher kann während einer Kopie umalloziert oder freigegeben werden. Auch Konfiguration und Alarmflags werden teils ungeschützt geteilt. Folge: inkonsistente Zustände und mögliches Heap-/Absturzproblem. Korrektur: konsistente Snapshots und denselben Mutex für alle beteiligten Lese- und Schreibzugriffe verwenden; Netzwerkaufrufe und Tonfolgen außerhalb der Sperre ausführen. Ein tatsächlicher Hardwareabsturz wurde nicht provoziert.

**3 · P1 · Separate kritische Owlet-Alarme werden verworfen — reproduziert.**

Die Property-Liste in [owlet.cpp:254](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/owlet.cpp:254) berücksichtigt weder `CRIT_OX_ALRT` noch `CRIT_BATT_ALRT`. Beide stehen in der aktuellen [Property-Zuordnung von pyowletapi](https://github.com/ryanbdclark/pyowletapi/blob/main/src/pyowletapi/const.py) und werden auch von dessen [Home-Assistant-Integration](https://github.com/ryanbdclark/owlet/blob/main/custom_components/owlet/binary_sensor.py) separat verarbeitet. Wenn eine Socke nur den kritischen Sauerstoffalarm setzt, erzeugt Owlanzi daraus weder Alarmtext noch Ton. Die Probe mit `CRIT_OX_ALRT=1` bestätigt das. Korrektur: separate Properties aufnehmen und ihre Alarmbehandlung ausdrücklich definieren; die Bedeutung eines kritischen Batteriehinweises nicht stillschweigend mit einem normalen niedrigen Akkustand gleichsetzen. Ob dein konkretes Modell diese Properties liefert, ist ohne dessen Rohantwort offen.

**4 · P1 · Frische wird aus Zahlenänderungen statt dem Messzeitpunkt abgeleitet — reproduziert.**

Der Filter in [owlet.cpp:238](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/owlet.cpp:238) verwirft `data_updated_at`; [owlet.cpp:287](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/owlet.cpp:287) merkt nur Änderungen von Puls oder Sauerstoff. [config.cpp:120](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/config.cpp:120) verwendet diese Änderung als Frischenachweis. Daraus entstehen drei nachgewiesene Fehlverhalten:

- Nach Neustart gilt ein alter Cloudwert als neu, weil sich der erste Puls-/Sauerstoffwert von den Initialwerten `-1` unterscheidet. Ein aktueller HTTP-Abruf beweist keine aktuelle Messung.
- Neue Messungen mit unverändertem Puls und Sauerstoff werden nach 60 Sekunden ausgeblendet, obwohl sich ihr Server-Messzeitpunkt weiterentwickelt.
- Nach dem Laden sperrt die Matrix alte Werte, während aktivierte eigene Alarme dieselben Werte auswerten: [main.cpp:97](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/main.cpp:97) prüft nur das Abrufalter und umgeht die Sitzungssperre. Im Test entsteht ein eigener Sauerstoffalarm, während `vitalsFresh()` weiterhin `false` liefert.

Korrektur: Messalter, Abrufalter und Sitzungsbeginn getrennt erfassen und daraus eine gemeinsame Datenfreigabe bilden. `data_updated_at` wird bereits von [pyowletapi/sock.py](https://github.com/ryanbdclark/pyowletapi/blob/main/src/pyowletapi/sock.py) ausgewertet. Die genaue Aktualisierungssemantik dieses Zeitstempels muss anschließend mit echten Antworten bei Laden, Abnehmen und konstanten Messwerten verifiziert werden. Einfach `vitalsFresh()` in die Alarmprüfung einzubauen wäre unzureichend, weil dessen bestehende Änderungsheuristik stabile Messungen ablehnt.

**5 · P1 · „Ton quittieren“ versteckt den Alarm und kann neue Alarmursachen stummschalten — reproduziert.**

[display.cpp:311](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/display.cpp:311) überspringt einen kritischen Alarm vollständig, sobald `silenced` gesetzt ist. Damit verschwinden Alarmtext und Alarmdarstellung von der Matrix, obwohl der Button nur das Quittieren des Tons verspricht. Zusätzlich setzt [config.cpp:165](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/config.cpp:165) die Quittierung erst zurück, wenn überhaupt kein kritischer Alarm mehr vorliegt. Kommt zu einem quittierten Alarm eine neue kritische Ursache hinzu, bleibt auch sie quittiert. Korrektur: sichtbaren Alarm, akustische Quittierung und Identität der quittierten Ursachen getrennt verwalten; neue Ursachen müssen erneut alarmieren können.

**6 · P1 · Testanzeigen und Startmeldungen haben Vorrang vor echten Alarmen — reproduziert für Vorschau.**

[display.cpp:289](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/display.cpp:289) zeichnet zuerst Testmodus, dann Meldungen und Setup; erst danach wird der Alarm geprüft. Ein aktiver Test mit Fantasiewerten kann dadurch einen realen kritischen Alarm 25 Sekunden verdecken, fortgesetzte Farbänderungen verlängern die Vorschau. Die Helligkeitswahl bevorzugt ebenfalls `briTest`. Korrektur: echte kritische Alarme vor Vorschauen und normalen Startmeldungen behandeln und die Vorschau bei ihrem Auftreten beenden. Der Test mit aktivem Sauerstoffalarm und `TEST_VITALS` bleibt fälschlich auf `SCR_TEST`.

**7 · P1 · OTA-Abschluss kann ohne Anmeldung einen Neustart auslösen — reproduziert.**

Der Handler in [webui.cpp:1370](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/webui.cpp:1370) hat kein `guard()` und bewertet allein `!Update.hasError()` als Erfolg. Ein leerer POST nach Neustart, bei dem kein Upload-Callback läuft, sieht den initialen Fehlerstatus 0 und startet das Gerät neu — auch bei gesetztem Webpasswort. Das ist ein Authentifizierungsfehler; die lokale Probe bestätigt den Kontrollfluss. Es wurde kein solcher Request an echte Hardware gesendet.

Im Upload-Callback wird außerdem nur bei `UPLOAD_FILE_START` authentifiziert; `UPLOAD_FILE_ABORTED` fehlt vollständig. Die geprüfte Arduino-ESP32-Bibliothek lässt bei einem abgebrochenen Transfer den begonnenen Updatezustand bis zum expliziten Abbruch bestehen. Korrektur: die gesamte OTA-Anfrage authentifizieren, Erfolg nur nach tatsächlich erfolgreichem Abschluss dieses Uploads melden und bei Abbruch `Update.abort()` ausführen. Nur einen fehlenden Bibliotheksfehler zu prüfen ist kein Erfolgsnachweis.

**8 · P2 · Verbindungsunterbrechungen können zur Alarmdauer zählen — reproduziert.**

In [main.cpp:127](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/main.cpp:127) laufen Login und Wiederanmeldung mit `continue` an der Alarmauswertung vorbei. Die `*Since`-Zeitpunkte bleiben erhalten. Reproduktion: ein niedriger Wert startet einen 30-Sekunden-Timer; fünf schnelle Fehlpolls innerhalb von 25 Sekunden erzwingen die Wiederanmeldung; nach mehreren Minuten ohne Messung kommt wieder ein niedriger Wert. Der Alarm löst sofort aus, obwohl keine durchgängige Unterschreitung für die eingestellte Dauer belegt ist. Korrektur: Unterbrechungen der Messreihe explizit behandeln und Daueralarme erst aus einer wieder zusammenhängenden Folge gültiger Messungen ableiten.

**9 · P2 · Ein alter Akkuhinweis kann „Offline“ dauerhaft verdecken — reproduziert.**

In [display.cpp:311](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/display.cpp:311) wird jeglicher vorhandener Alarm-/Hinweistext vor der Verbindungsprüfung angezeigt. Bleibt `LOW_BATT_ALRT` beim Verbindungsabbruch gesetzt, läuft der Akkuhinweis auch nach Minuten weiter; der Offline-Zweig wird nicht erreicht. Die Probe nach 120 Sekunden ohne Daten bleibt auf dem Hinweisbildschirm. Korrektur: veraltete Hinweise dürfen einen Verbindungsverlust nicht verdrängen. Bei einem zuletzt bekannten kritischen Alarm sollte sowohl dieser als auch die fehlende Aktualität erkennbar bleiben.

**10 · P2 · Eigene Alarme werden beim Abschalten nicht sofort gelöscht — reproduziert.**

[webui.cpp:1269](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/webui.cpp:1269) speichert `ownAlarms=false` und ruft nur `alarmRecompute()` auf. Diese Funktion übernimmt weiterhin die bereits gesetzten `gSt.alSpo2/alHrLow/alHrHigh`. Erst eine spätere `evalAlarms()`-Ausführung löscht sie. Im normalen Betrieb bleibt der Alarm bis zum nächsten Auswertungsdurchlauf bestehen, bei längerer Wiederanmeldung möglicherweise wesentlich länger. Korrektur: eigene Alarmflags und ihre Timer beim Deaktivieren unmittelbar und atomar zurücksetzen; Owlet-eigene Alarme davon unabhängig erhalten.

**11 · P2 · Regionenwechsel invalidiert die Anmeldung nicht — reproduziert.**

[webui.cpp:1273](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/webui.cpp:1273) erkennt nur E-Mail- und Passwortänderungen. Beim Wechsel zwischen Europa und Welt ändern sich die Endpoints sofort, während alter Token, `loggedIn` und DSN erhalten bleiben. Die nächsten Aufrufe gehen mit Zugangsdaten der bisherigen Region an die andere Region; eine Wiederanmeldung wird erst indirekt nach wiederholten Fehlern erzwungen. Korrektur: die Region zur Identität der Anmeldung zählen und den Wechsel gemeinsam mit Token-/Geräte-/Messzustand behandeln. Laufende Requests müssen an ihren Konfigurationsstand gebunden sein, damit sie einen zwischenzeitlichen Wechsel nicht überschreiben.

**12 · P2 · Alarmgrenzen und Dauern werden serverseitig nicht validiert — reproduziert für negative Dauer.**

[webui.cpp:1255](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/webui.cpp:1255) übernimmt Ganzzahlen ungeprüft. Nur das Pollintervall wird später begrenzt. Die Oberfläche ruft `save()` direkt per Button auf und löst keine Formularvalidierung aus; HTML-`min/max` reichen daher nicht. `spo2Seconds=-1` wird gespeichert und durch die vorzeichenlose Zeitrechnung praktisch zu einer Wartezeit von rund 49,7 Tagen; eine leere Dauer wird im Browser zu 0 und löst sofort aus. Korrektur: Typ, Wertebereich und zusammengehörige Unter-/Obergrenzen vor dem Speichern prüfen; ungültige Konfiguration mit nachvollziehbarer Fehlermeldung ablehnen.

**13 · P2 · Fehler beim Aktivieren der Cloud-Aktualisierung werden verschluckt — statisch belegt.**

[owlet.cpp:230](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/owlet.cpp:230) ignoriert den Rückgabewert des `APP_ACTIVE`-Requests. Ein anschließender erfolgreicher Property-Abruf setzt `cloudOk=true`, löscht `lastError` und setzt den Fehlerzähler zurück. Fällt nur der Aktivierungsrequest aus, bleibt die Ursache unsichtbar und es können gecachte Messwerte weiter abgeholt werden. Korrektur: Aktivierungsfehler separat erfassen und erneut versuchen; Messwerte nur anhand ihres tatsächlichen Alters freigeben. Bei der Erfolgsprüfung auch gültige Antworten ohne JSON-Nutzlast berücksichtigen, statt einen solchen Transporterfolg als JSON-Fehler zu behandeln.

**Weitere konkrete Grenzen aus der vollständigen Durchsicht**

- **Mehrere Socken/Geräte:** [owlet.cpp:208](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/src/owlet.cpp:208) nimmt immer `devices[0]`. Eine Auswahl oder Prüfung des Gerätetyps fehlt. Bei mehreren Socken ist damit nicht festgelegt, welches Kind angezeigt wird; bei einem anderen Gerät an erster Stelle kann `REAL_TIME_VITALS` fehlen. Das ist eine Einschränkung der aktuellen Ein-Gerät-Annahme; ein Konto mit mehreren Geräten wurde nicht live geprüft.
- **Langzeitbetrieb:** `retryAt`, Tokenablauf, Test- und Meldungsende verwenden teilweise absolute Vergleiche mit dem umlaufenden `millis()`-Zähler. Insbesondere kann eine über den Überlauf laufende fehlgeschlagene Anmeldung ein vor dem Überlauf liegendes, großes `retryAt` hinterlassen und weitere Anmeldungen sehr lange sperren. Zeitdifferenzen müssen überlaufsicher verglichen werden. Kein Hardware-Dauertest durchgeführt.
- **WLAN-Status:** `wifiOk` wird nur beim Start gesetzt. Spätere Cloudfehler führen zwar schließlich zur Offline-Anzeige und Wi-Fi-Autoreconnect ist aktiviert, der interne WLANstatus bildet spätere Trennungen aber nicht ab.
- **Webanzeige bei Geräteausfall:** `tick()` und `frame()` kehren bei Fetch-Fehlern ohne sichtbare Fehlerkennzeichnung zurück. Die zuletzt angezeigten Werte und das Spiegelbild bleiben im Browser stehen. Die Matrix hat dagegen eine eigene Zeitprüfung. Ein sichtbares Alter seit dem letzten erfolgreichen Browserabruf fehlt.
- **Blockierende Bedienung:** WLANscan, Upload und wiederholte Tonmelodie laufen im selben Loop wie die Matrix. Insbesondere `soundAlarm()` blockiert dort rund 720 ms. Der getrennte Netzwerk-Task verhindert diese Pausen nicht.

**Build, Installer und Prüfungen**

Der Release-Build mit `NO_LOCAL_SECRETS` besteht: Espressif32 6.8.1, Arduino-ESP32 2.0.17, ArduinoJson 7.4.3, FastLED 3.10.3. Statischer RAMbedarf: 54.464 von 327.680 Bytes; Programm: 1.195.069 von 1.966.080 Bytes; erzeugte App-Datei: 1.201.648 Bytes. Es gibt eine Makrowarnung für das lokale `PS`-Makro in `config.cpp`, das mit einem Xtensa-Header kollidiert; der Build scheitert daran nicht.

| Prüfung | Ergebnis |
| --- | --- |
| Release kompilieren und linken | bestanden |
| Vorhandene Installer-Regressionstests | 6 von 6 bestanden |
| Zusätzliche C++-Verhaltensprüfungen | 21 Aussagen geprüft: 4 bestanden, 17 fehlgeschlagen; mehrere Aussagen betreffen denselben Befund |
| JavaScript-Syntax für vollständige Haupt- und Setup-Seite | bestanden |
| Übersetzungsaudit Hauptseite | 129 Textschlüssel und 1 Platzhalterschlüssel vollständig vorhanden |
| Frisch gebaute App und drei vorhandene Installer-Dateien | Image-Prüfsummen, 4-MB-Grenzen und passende OTA-Slots bestanden |
| Vergleich gegen lokale private Seed-Werte | alle 3 Zugangsdatenwerte in allen 4 geprüften Artefakten abwesend |
| Cppcheck über alle fünf Implementierungsdateien | keine weiteren belastbaren Laufzeitbefunde; ArduinoJson-`operator|` wird ohne Bibliothekstypwissen fälschlich als Bitmaske beanstandet |

Die Verhaltensprüfung kompiliert die echten Dateien `config.cpp` und `display.cpp` sowie extrahierte Originalfunktionen für Parsing, Alarmauswertung, Schlafnamen, Konfigurationsspeicherung und OTA-Abschluss. Arduino-Strings, Uhrzeit, NVS, Netzwerk und Hardware sind lokal nachgebildet; ArduinoJson kommt aus der verwendeten Release-Abhängigkeit. Das verifiziert die beschriebenen Verzweigungen, ersetzt aber keine ESP32-Tests für Parallelität, WLAN, Sensorhardware oder Flash-Schreibvorgänge.

Die Review-Werkzeuge und Ausgaben liegen absichtlich im bereits ignorierten Buildverzeichnis:

- [Generator der C++-Prüfungen](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/.pio/review/make_probes.py)
- [Compiler-/Testaufruf](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/.pio/review/run-probes.cmd)
- [Einzelne Prüfergebnisse](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/.pio/review/probe-results.log)
- [Binär- und Seedprüfung](C:/Users/Jannik/IdeaProjects/privat/owlanzi/owlanzi-firmware/.pio/review/audit_artifacts.py)

**Wichtig für eine spätere korrigierte Veröffentlichung:** Die Website baut standardmäßig aus ihrem eigenen Snapshot in `website-tools/release/src`. Vier Dateien unterscheiden sich vom hier geprüften Firmwarestand: `config.cpp`, `display.cpp`, `owlanzi.h`, `webui.cpp`. Der Website-Snapshot enthält unter anderem noch die Serpentinenoption und ältere Farb-/Vorschaufunktionen. Der mitgelieferte TC001-Installer im Firmwareprojekt ist bytegleich mit dem Website-TC001-Installer 1.0.2; sein SHA-256 ist `f8ed9b7805fe887b232d1c2714d8ea765b91746e6ade2f15a81599c5a09df444`. Ein erfolgreiches Kompilieren des aktuellen Firmwareprojekts aktualisiert diese Installer nicht. Bei der Veröffentlichung muss der geprüfte Quellstand ausdrücklich übernommen werden, beispielsweise über den vorgesehenen Parameter `FirmwareSourceProject` des Website-Buildskripts. Die bestehende Versionsdifferenz allein ist kein Beweis für einen defekten Releaseprozess.

**Empfohlene Reihenfolge der Korrekturen:** zuerst gemeinsame Schlafcode-Zuordnung; dann konsistente Zustandsverwaltung, Alarm-/Quittierungslogik und Messaktualität; anschließend OTA-Authentifizierung und Abbruchbehandlung; danach Konfiguration und Wiederverbindung. Vor einem neuen Installer müssen die nachgewiesenen Fehlerfälle als echte Regressionstests bestehen und die Übergänge Laden → Messen, Verbindungsverlust → Wiederanmeldung sowie Alarm → Quittierung → neue Alarmursache auf einem Testgerät überprüft werden.
