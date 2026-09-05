<#
  logo-embed.ps1 - baut aus assets\branding\owlanzi.svg das PROGMEM-Literal
  LOGO in src\webui.cpp.

  Warum ein Skript statt Handarbeit: die Grafik kommt aus dem Zeichenprogramm
  mit 13 kB Einrueckung und 169 einzelnen fill-Attributen. Auf dem ESP32
  zaehlt jedes Byte doppelt - es liegt im Flash UND geht bei jedem
  Seitenaufruf ueber WLAN. Hier wandert das Fuellattribut in die Gruppe und
  der Leerraum faellt weg; gezeichnet wird Pixel fuer Pixel dasselbe Bild.
  Kommt eine neue Fassung des Logos, einfach nochmal laufen lassen.

  ---------------------------------------------------------------------------
  PowerShell-Fallen, in die dieses Projekt schon dreimal getreten ist:

   1. IMMER als Datei mit -File starten. Bei  powershell -Command "..."
      frisst die Aufrufkette die Dollarzeichen; uebrig bleibt Kauderwelsch
      und eine Fehlermeldung, die woanders hinzeigt.
   2. Variablennamen sind NICHT case-sensitiv. $a und $A sind ein und
      dieselbe Variable - so kamen einmal alle Umlaute gross heraus.
      Darum hier durchweg sprechende Namen.
   3. Im Array-Literal bindet das Komma staerker als das Plus:
      ('a' + $x, 'b') ist EIN Element, nicht zwei.
  ---------------------------------------------------------------------------
#>
param(
  [string]$Logo = "$PSScriptRoot\..\assets\branding\owlanzi.svg",
  [string]$Cpp  = "$PSScriptRoot\..\src\webui.cpp"
)
$ErrorActionPreference = 'Stop'

function Show([string]$text) { Write-Output $text }

$svgRaw = [System.IO.File]::ReadAllText($Logo)
Show ("Quelle : {0}  ({1} Bytes)" -f $Logo, $svgRaw.Length)

# --- viewBox uebernehmen, nicht raten -------------------------------------
$viewBox = [regex]::Match($svgRaw, '<svg[^>]*viewBox="([^"]*)"').Groups[1].Value
if (-not $viewBox) { throw 'Kein viewBox im Logo gefunden.' }

# --- Kacheln der Pixel-Eule einsammeln ------------------------------------
$owlGroup = [regex]::Match($svgRaw, '<g id="pixel-owl">(.*?)</g>', 'Singleline').Groups[1].Value
if (-not $owlGroup) { throw 'Gruppe pixel-owl nicht gefunden.' }

$tileList = @()
foreach ($hit in [regex]::Matches($owlGroup, '<rect\s+([^/>]*)/>')) {
  $attr = @{}
  foreach ($pair in [regex]::Matches($hit.Groups[1].Value, '([\w-]+)="([^"]*)"')) {
    $attr[$pair.Groups[1].Value] = $pair.Groups[2].Value
  }
  $tileList += , $attr
}
if ($tileList.Count -lt 100) { throw "Nur $($tileList.Count) Kacheln - der Aufbau der Datei hat sich geaendert." }

# Alle Kacheln muessen gleich gross sein, sonst darf width/height/rx nicht
# aus der ersten abgeschrieben werden.
$shapes = $tileList | ForEach-Object { "$($_.width)/$($_.height)/$($_.rx)" } | Sort-Object -Unique
if ($shapes.Count -ne 1) { throw ("Kacheln unterschiedlich gross: " + ($shapes -join ', ')) }

$tileW  = $tileList[0].width
$tileH  = $tileList[0].height
$tileRx = $tileList[0].rx

# Positionen muessen eindeutig sein - nur dann ist das Umsortieren nach Farbe
# folgenlos (spaeter gezeichnete Rechtecke lagen sonst oben).
$places = $tileList | ForEach-Object { "$($_.x),$($_.y)" } | Sort-Object -Unique
if ($places.Count -ne $tileList.Count) { throw 'Kacheln ueberlappen sich - Umsortieren nach Farbe waere nicht folgenlos.' }

$fillGroups = $tileList | Group-Object { $_.fill } | Sort-Object Count -Descending
$mainFill   = $fillGroups[0].Name
Show ("Kacheln: {0}, Groesse {1}x{2} rx {3}, Farben: {4}" -f $tileList.Count, $tileW, $tileH, $tileRx,
      (($fillGroups | ForEach-Object { "$($_.Name) x$($_.Count)" }) -join ', '))

# Vor dem "/>" MUSS ein Leerzeichen stehen. Ohne Anfuehrungszeichen schluckt
# der HTML-Parser den Schraegstrich sonst als Teil des Attributwerts - das
# Element schliesst sich nicht selbst und alles Weitere landet verschachtelt.
$build = New-Object System.Text.StringBuilder
foreach ($tile in $tileList) {
  if ($tile.fill -ne $mainFill) { continue }
  [void]$build.Append("<rect x=$($tile.x) y=$($tile.y) width=$tileW height=$tileH rx=$tileRx />")
}
foreach ($tile in $tileList) {
  if ($tile.fill -eq $mainFill) { continue }
  [void]$build.Append("<rect x=$($tile.x) y=$($tile.y) width=$tileW height=$tileH rx=$tileRx fill=""$($tile.fill)"" />")
}

# --- Wortmarke unveraendert, nur ohne Zeilenumbrueche ----------------------
$wordmark = [regex]::Match($svgRaw, '<g id="wordmark".*?</g>', 'Singleline').Value
if (-not $wordmark) { throw 'Gruppe wordmark nicht gefunden.' }
$wordmark = [regex]::Replace($wordmark, '\s*\r?\n\s*', '')

# fill=none gehoert an die Wurzel: die Wortmarke besteht nur aus Strichen.
# Ohne das Attribut wuerde der Browser ihre Pfade schwarz ausfuellen.
$svgOut = '<svg viewBox="' + $viewBox + '" xmlns="http://www.w3.org/2000/svg" fill=none' +
          ' class=logo role=img aria-label=owlanzi>' +
          '<g fill="' + $mainFill + '">' + $build.ToString() + '</g>' + $wordmark + '</svg>'

if ($svgOut -match '[\r\n]') { throw 'Das erzeugte SVG enthaelt Zeilenumbrueche.' }
Show ("SVG    : {0} Bytes (aus {1}, also {2} %)" -f $svgOut.Length, $svgRaw.Length,
      [int](100 * $svgOut.Length / $svgRaw.Length))

# --- in webui.cpp einsetzen ------------------------------------------------
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$cppText   = [System.IO.File]::ReadAllText($Cpp, $utf8NoBom)

$literalRx = '(?m)^static const char (?:OWL|LOGO)\[\] PROGMEM = R"(?:OWL|LOGO)\(.*\)(?:OWL|LOGO)";'
$found     = [regex]::Matches($cppText, $literalRx)
if ($found.Count -ne 1) { throw "Erwartet: genau ein Logo-Literal, gefunden: $($found.Count)." }
$oldLen = $found[0].Value.Length

$newLine   = 'static const char LOGO[] PROGMEM = R"LOGO(' + $svgOut + ')LOGO";'
$evaluator = [System.Text.RegularExpressions.MatchEvaluator] { param($m) $newLine }
$cppNew    = [regex]::Replace($cppText, $literalRx, $evaluator)

# Das Literal wurde gefunden (sonst waere oben schon abgebrochen worden).
# Gleicher Text heisst also: schon auf dem Stand der Grafik - kein Fehler.
if ($cppNew -eq $cppText) {
  Show 'Literal ist bereits auf dem Stand der Grafik - nichts zu tun.'
  exit 0
}

[System.IO.File]::WriteAllText("$Cpp.bak", $cppText, $utf8NoBom)
[System.IO.File]::WriteAllText($Cpp,       $cppNew,  $utf8NoBom)

Show ("Literal: {0} -> {1} Bytes" -f $oldLen, $newLine.Length)
Show ("Datei  : {0} -> {1} Bytes (Sicherung als webui.cpp.bak)" -f $cppText.Length, $cppNew.Length)

# --- Kontrolle: Umlaute unbeschaedigt? ------------------------------------
$census = @{}
foreach ($ch in $cppNew.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $key = 'U+{0:X4}' -f [int]$ch
    if ($census.ContainsKey($key)) { $census[$key]++ } else { $census[$key] = 1 }
  }
}
Show ("Sonderzeichen: " + (($census.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Key) x$($_.Value)" }) -join ', '))
