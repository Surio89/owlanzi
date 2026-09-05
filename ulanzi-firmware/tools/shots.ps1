<#
  shots.ps1 - Kopfzeile der Oberflaeche in mehreren Fensterbreiten ablichten.
  Prueft, ob Logo, Zustandsmarke und die Sprachumschaltung auch auf schmalen
  Telefonen nebeneinander passen.

  Immer als Datei starten (-File). Bei -Command frisst die Aufrufkette die
  Dollarzeichen, und "foreach (" ohne Variablenname bricht ab.
#>
param(
  [string]$Ip     = '192.168.100.13',
  [int[]] $Breite = @(320, 390, 560, 900),
  [int]   $Hoehe  = 200
)
$ErrorActionPreference = 'Stop'
$wurzel = "$PSScriptRoot\.."
$shot   = Join-Path $wurzel 'tools\shot.ps1'

Start-Sleep -Seconds 8   # der ESP32 braucht nach dem Reset einen Moment

foreach ($b in $Breite) {
  $ziel = Join-Path $wurzel ("w{0}.png" -f $b)
  $ErrorActionPreference = 'Continue'
  & powershell -NoProfile -ExecutionPolicy Bypass -File $shot `
      -Url ("http://{0}/" -f $Ip) -Width $b -Height $Hoehe -Out $ziel 2>$null | Out-Null
  $ErrorActionPreference = 'Stop'
  if (Test-Path $ziel) { Write-Output ("{0,4} px -> {1} ({2} Bytes)" -f $b, (Split-Path $ziel -Leaf), (Get-Item $ziel).Length) }
  else                 { Write-Output ("{0,4} px -> kein Bild" -f $b) }
}
