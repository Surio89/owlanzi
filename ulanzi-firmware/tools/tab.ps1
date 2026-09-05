<#
  tab.ps1 - lichtet einen Reiter der Oberflaeche ab, direkt vom Geraet.

  Seit die Reiter in der Adresse stehen (#display), reicht dafuer die URL.
  Eine lokale Kopie waere hier falsch: sie laeuft unter file:// und darf die
  Schnittstellen des Geraets nicht abfragen - die Vorschauen blieben leer.

  Reiter: status, display, alarms, system.
  Immer als Datei starten (-File).
#>
param(
  [string]$Ip     = '192.168.100.13',
  [ValidateSet('status','display','alarms','system')]
  [string]$Tab    = 'display',
  [int]   $Width  = 820,
  [int]   $Height = 1500,
  [string]$Out    = ''
)
$ErrorActionPreference = 'Stop'
$wurzel = "$PSScriptRoot\.."
if (-not $Out) { $Out = Join-Path $wurzel ("tab-{0}.png" -f $Tab) }

$ErrorActionPreference = 'Continue'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $wurzel 'tools\shot.ps1') `
    -Url ("http://{0}/#{1}" -f $Ip, $Tab) -Width $Width -Height $Height `
    -Scale 1 -Out $Out 2>$null | Out-Null
$ErrorActionPreference = 'Stop'

if (Test-Path $Out) { Write-Output ("{0} ({1} Bytes), Reiter {2}" -f (Split-Path $Out -Leaf), (Get-Item $Out).Length, $Tab) }
else { Write-Output 'Kein Bild entstanden.' }
