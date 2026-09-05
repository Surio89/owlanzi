<#
  tab-de.ps1 - lichtet einen Reiter auf Deutsch ab.

  Die Sprache liegt auf dem Geraet, nicht im Browser. Also: kurz auf Deutsch
  stellen, Bild machen, wieder zurueckstellen. Nur fuers Entwicklerboard
  gedacht - an der Uhr am Kinderbett hat das nichts zu suchen.

  Immer als Datei starten (-File).
#>
param(
  [string]$Ip     = '192.168.100.13',
  [ValidateSet('status','display','alarms','system')]
  [string]$Tab    = 'alarms',
  [int]   $Width  = 820,
  [int]   $Height = 1400
)
$ErrorActionPreference = 'Stop'
$wurzel = "$PSScriptRoot\.."

function Sprache([string]$l) {
  Invoke-WebRequest -Uri ("http://{0}/api/config" -f $Ip) -Method Post `
    -ContentType 'application/json' -Body ('{"lang":"' + $l + '"}') `
    -TimeoutSec 15 -UseBasicParsing | Out-Null
  Start-Sleep -Milliseconds 400
}

$vorher = (Invoke-RestMethod -Uri ("http://{0}/api/config" -f $Ip) -TimeoutSec 15).lang
Write-Output ("Sprache vorher: " + $vorher)

Sprache 'de'
try {
  $out = Join-Path $wurzel ("tab-{0}-de.png" -f $Tab)
  $ErrorActionPreference = 'Continue'
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $wurzel 'tools\shot.ps1') `
      -Url ("http://{0}/#{1}" -f $Ip, $Tab) -Width $Width -Height $Height `
      -Scale 1 -Out $out 2>$null | Out-Null
  $ErrorActionPreference = 'Stop'
  if (Test-Path $out) { Write-Output ("{0} ({1} Bytes)" -f (Split-Path $out -Leaf), (Get-Item $out).Length) }
} finally {
  Sprache $vorher
  Write-Output ("Sprache wieder auf: " + (Invoke-RestMethod -Uri ("http://{0}/api/config" -f $Ip) -TimeoutSec 15).lang)
}
