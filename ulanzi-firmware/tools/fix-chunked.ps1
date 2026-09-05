$ErrorActionPreference='Stop'
$p = Join-Path $PSScriptRoot "..\src\main.cpp"
$c = Get-Content $p -Raw

$alt = @'
  DeserializationError e = filter
    ? deserializeJson(out, http.getStream(),
                      DeserializationOption::Filter(*filter))
    : deserializeJson(out, http.getStream());
'@

$neu = @'
  /*
   * WICHTIG - hier ist die Anmeldung am 4.9. gescheitert:
   * HTTPClient::getStream() liefert bei "Transfer-Encoding: chunked" die
   * Chunk-Laengenmarker MIT im Datenstrom. ArduinoJson stolpert darueber und
   * gibt still ein leeres Dokument zurueck, ohne Fehler zu melden. Aus dem
   * 938 Zeichen langen JWT wurde so die Zeichenkette "null" - vier Zeichen,
   * die dann als Authorization-Header rausgingen. getString() entpackt die
   * Marker korrekt, braucht dafuer aber Speicher fuer die ganze Antwort.
   * Also: Content-Length bekannt -> streamen (speicherschonend),
   *       chunked -> puffern (und die Groesse mitloggen).
   */
  int clen = http.getSize();          // -1 = chunked, keine Content-Length
  DeserializationError e;
  if (clen >= 0) {
    e = filter ? deserializeJson(out, http.getStream(),
                                 DeserializationOption::Filter(*filter))
               : deserializeJson(out, http.getStream());
  } else {
    String payload = http.getString();
    Serial.printf("   .. %s: chunked, %u Bytes gepuffert (Heap frei %u)\n",
                  label, (unsigned)payload.length(),
                  (unsigned)ESP.getFreeHeap());
    e = filter ? deserializeJson(out, payload,
                                 DeserializationOption::Filter(*filter))
               : deserializeJson(out, payload);
  }
'@

if (-not $c.Contains($alt)) { Write-Output "Textblock nicht gefunden - nichts geaendert"; exit 1 }
Copy-Item $p ($p + ".vor-chunked-fix") -Force
$c = $c.Replace($alt, $neu)
Set-Content $p $c -NoNewline
Write-Output "Chunked-Behandlung eingebaut."
