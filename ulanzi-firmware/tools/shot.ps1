<#
  shot.ps1 - Bildschirmfoto einer Seite ohne sichtbares Fenster.

  Damit laesst sich pruefen, wie die ausgelieferte Oberflaeche wirklich
  aussieht, statt nur den Quelltext zu lesen. Nimmt Chrome, sonst Edge.

  Aufruf (immer als Datei, siehe Falle 1 in logo-embed.ps1):
    powershell -File tools\shot.ps1 -Url "http://192.168.100.13/" -Width 900
#>
param(
  [string]$Url    = 'http://192.168.100.13/',
  [int]   $Width  = 900,
  [int]   $Height = 700,
  [string]$Out    = "$PSScriptRoot\..\shot.png",
  [int]   $Scale  = 2,
  # Der neue Headless-Modus erzwingt eine Fensterbreite von mindestens 500 px -
  # schmalere Angaben werden stillschweigend auf 500 gehoben und das Bild
  # danach nur zugeschnitten. Wer Telefonbreiten prueft, nimmt 'old'.
  [ValidateSet('new', 'old')]
  [string]$Headless = 'new'
)
$ErrorActionPreference = 'Stop'

$kandidaten = @(
  'C:\Program Files\Google\Chrome\Application\chrome.exe',
  'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe',
  'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
  'C:\Program Files\Microsoft\Edge\Application\msedge.exe'
)
$browser = $kandidaten | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $browser) { throw 'Weder Chrome noch Edge gefunden.' }

if (Test-Path $Out) { Remove-Item $Out }

$profil = Join-Path $env:TEMP ('owlanzi-shot-' + [guid]::NewGuid().ToString('N'))
$argumente = @(
  "--headless=$Headless"
  '--disable-gpu'
  '--hide-scrollbars'
  "--user-data-dir=$profil"
  "--window-size=$Width,$Height"
  "--force-device-scale-factor=$Scale"
  # Grosszuegig: die Seite holt erst die Einstellungen, dann zeichnet sie die
  # Vorschauen. Zu knapp bemessen entsteht ein Bild mit schwarzen Kaesten.
  '--virtual-time-budget=9000'
  "--screenshot=$Out"
  $Url
)
# Chrome meldet den Erfolg auf dem Fehlerkanal ("... bytes written to file").
# Bei ErrorActionPreference=Stop wuerde PowerShell das als Abbruch werten,
# also hier ausdruecklich weitermachen und den Kanal wegwerfen.
$ErrorActionPreference = 'Continue'
& $browser @argumente 2>$null | Out-Null
$ErrorActionPreference = 'Stop'

if (Test-Path $profil) { Remove-Item $profil -Recurse -Force -ErrorAction SilentlyContinue }

if (Test-Path $Out) {
  $groesse = (Get-Item $Out).Length
  Write-Output ("{0} ({1} Bytes) mit {2}" -f $Out, $groesse, (Split-Path $browser -Leaf))
} else {
  throw 'Kein Bild entstanden.'
}
