<#
  tab-de.ps1 - screenshot one tab with the interface set to German.

  The language lives on the device, not in the browser. So: switch to German
  briefly, take the picture, switch back. Meant for the dev board only - it
  has no business touching the clock beside the cot.

  Always run as a file (-File).
#>
param(
  [string]$Ip     = '192.168.100.13',
  [ValidateSet('status','display','alarms','system')]
  [string]$Tab    = 'alarms',
  [int]   $Width  = 820,
  [int]   $Height = 1400
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

function Set-Lang([string]$l) {
  Invoke-WebRequest -Uri ("http://{0}/api/config" -f $Ip) -Method Post `
    -ContentType 'application/json' -Body ('{"lang":"' + $l + '"}') `
    -TimeoutSec 15 -UseBasicParsing | Out-Null
  Start-Sleep -Milliseconds 400
}

$before = (Invoke-RestMethod -Uri ("http://{0}/api/config" -f $Ip) -TimeoutSec 15).lang
Write-Output ("language before: " + $before)

Set-Lang 'de'
try {
  $out = Join-Path $root ("tab-{0}-de.png" -f $Tab)
  $ErrorActionPreference = 'Continue'
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tools\shot.ps1') `
      -Url ("http://{0}/#{1}" -f $Ip, $Tab) -Width $Width -Height $Height `
      -Scale 1 -Out $out 2>$null | Out-Null
  $ErrorActionPreference = 'Stop'
  if (Test-Path $out) { Write-Output ("{0} ({1} bytes)" -f (Split-Path $out -Leaf), (Get-Item $out).Length) }
} finally {
  Set-Lang $before
  Write-Output ("language restored to: " + (Invoke-RestMethod -Uri ("http://{0}/api/config" -f $Ip) -TimeoutSec 15).lang)
}
