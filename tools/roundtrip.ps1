<#
  roundtrip.ps1 - checks that saving and loading back really agree.

  Sends exactly the fields the interface sends, reads them back, and restores
  the original state at the end. Wi-Fi and account fields stay out of it: a
  Wi-Fi change would reboot the device.

  Always run as a file (-File). Dev board only.
#>
param([string]$Ip = '192.168.100.13')
$ErrorActionPreference = 'Stop'
$url = "http://$Ip/api/config"

function Get-Cfg { Invoke-RestMethod -Uri $url -TimeoutSec 20 }
function Set-Cfg([hashtable]$h) {
  $body = $h | ConvertTo-Json -Compress -Depth 5
  Invoke-RestMethod -Uri $url -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 20 | Out-Null
  Start-Sleep -Milliseconds 400
}

$before = Get-Cfg
Write-Output ("before : pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}" -f
  $before.pollSeconds, $before.spo2Limit, $before.volAlarm, $before.pal.heart)

# 1) A round with valid values, exactly as the interface sends them.
Set-Cfg @{ pollSeconds = 15; spo2Limit = 90; volAlarm = 7; pal = @{ heart = 3407871 } }
$after = Get-Cfg
$ok1 = ($after.pollSeconds -eq 15 -and $after.spo2Limit -eq 90 -and $after.volAlarm -eq 7 -and $after.pal.heart -eq 3407871)
Write-Output ("after  : pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}   -> {4}" -f
  $after.pollSeconds, $after.spo2Limit, $after.volAlarm, $after.pal.heart, $(if ($ok1) { 'OK' } else { 'MISMATCH' }))

# 2) The guard: a 0 must not get through, or the device would call the Owlet
#    cloud in a loop.
Set-Cfg @{ pollSeconds = 0 }
$atZero = (Get-Cfg).pollSeconds
Set-Cfg @{ pollSeconds = 9999 }
$atMax = (Get-Cfg).pollSeconds
$ok2 = ($atZero -ge 3 -and $atMax -le 120)
Write-Output ("guard  : 0 -> {0}, 9999 -> {1}   -> {2}" -f $atZero, $atMax, $(if ($ok2) { 'OK' } else { 'OPEN' }))

# 3) Put the original state back.
Set-Cfg @{
  pollSeconds = [int]$before.pollSeconds; spo2Limit = [int]$before.spo2Limit
  volAlarm = [int]$before.volAlarm; pal = @{ heart = [int]$before.pal.heart }
}
$restored = Get-Cfg
$ok3 = ($restored.pollSeconds -eq $before.pollSeconds -and $restored.spo2Limit -eq $before.spo2Limit -and
        $restored.volAlarm -eq $before.volAlarm -and $restored.pal.heart -eq $before.pal.heart)
Write-Output ("restored: pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}   -> {4}" -f
  $restored.pollSeconds, $restored.spo2Limit, $restored.volAlarm, $restored.pal.heart,
  $(if ($ok3) { 'OK' } else { 'NOT RESTORED' }))
