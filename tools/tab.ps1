<#
  tab.ps1 - screenshot one tab of the interface, straight from the device.

  Since the tabs live in the address (#display) the URL is all it takes.
  A local copy would be wrong here: it runs under file:// and is not allowed
  to call the device endpoints - the previews would stay empty.

  Tabs: status, display, alarms, system.
  Always run as a file (-File).
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
$root = Split-Path $PSScriptRoot -Parent
if (-not $Out) { $Out = Join-Path $root ("tab-{0}.png" -f $Tab) }

$ErrorActionPreference = 'Continue'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tools\shot.ps1') `
    -Url ("http://{0}/#{1}" -f $Ip, $Tab) -Width $Width -Height $Height `
    -Scale 1 -Out $Out 2>$null | Out-Null
$ErrorActionPreference = 'Stop'

if (Test-Path $Out) { Write-Output ("{0} ({1} bytes), tab {2}" -f (Split-Path $Out -Leaf), (Get-Item $Out).Length, $Tab) }
else { Write-Output 'No image produced.' }
