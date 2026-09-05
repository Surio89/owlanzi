<#
  roundtrip.ps1 - prueft, dass Speichern und Zurueckladen wirklich passen.

  Schickt genau die Felder, die auch die Oberflaeche schickt, liest zurueck
  und stellt am Ende den Ausgangszustand wieder her. WLAN- und Kontodaten
  bleiben aussen vor: eine WLAN-Aenderung wuerde das Geraet neu starten.

  Immer als Datei starten (-File). Nur fuers Entwicklerboard.
#>
param([string]$Ip = '192.168.100.13')
$ErrorActionPreference = 'Stop'
$url = "http://$Ip/api/config"

function Hole { Invoke-RestMethod -Uri $url -TimeoutSec 20 }
function Schick([hashtable]$h) {
  $body = $h | ConvertTo-Json -Compress -Depth 5
  Invoke-RestMethod -Uri $url -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 20 | Out-Null
  Start-Sleep -Milliseconds 400
}

$vorher = Hole
Write-Output ("vorher : pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}" -f
  $vorher.pollSeconds, $vorher.spo2Limit, $vorher.volAlarm, $vorher.pal.heart)

# 1) Runde mit gueltigen Werten, so wie die Oberflaeche sie schickt.
Schick @{ pollSeconds = 15; spo2Limit = 90; volAlarm = 7; pal = @{ heart = 3407871 } }
$nach = Hole
$ok1 = ($nach.pollSeconds -eq 15 -and $nach.spo2Limit -eq 90 -and $nach.volAlarm -eq 7 -and $nach.pal.heart -eq 3407871)
Write-Output ("nachher: pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}   -> {4}" -f
  $nach.pollSeconds, $nach.spo2Limit, $nach.volAlarm, $nach.pal.heart, $(if ($ok1) { 'OK' } else { 'ABWEICHUNG' }))

# 2) Riegel: eine 0 darf nicht durchgehen, sonst ruft das Geraet die
#    Owlet-Cloud in einer Schleife an.
Schick @{ pollSeconds = 0 }
$null0 = (Hole).pollSeconds
Schick @{ pollSeconds = 9999 }
$nullMax = (Hole).pollSeconds
$ok2 = ($null0 -ge 3 -and $nullMax -le 120)
Write-Output ("Riegel : 0 -> {0}, 9999 -> {1}   -> {2}" -f $null0, $nullMax, $(if ($ok2) { 'OK' } else { 'OFFEN' }))

# 3) Ausgangszustand wiederherstellen.
Schick @{
  pollSeconds = [int]$vorher.pollSeconds; spo2Limit = [int]$vorher.spo2Limit
  volAlarm = [int]$vorher.volAlarm; pal = @{ heart = [int]$vorher.pal.heart }
}
$zurueck = Hole
$ok3 = ($zurueck.pollSeconds -eq $vorher.pollSeconds -and $zurueck.spo2Limit -eq $vorher.spo2Limit -and
        $zurueck.volAlarm -eq $vorher.volAlarm -and $zurueck.pal.heart -eq $vorher.pal.heart)
Write-Output ("zurueck: pollSeconds={0}  spo2Limit={1}  volAlarm={2}  heart={3}   -> {4}" -f
  $zurueck.pollSeconds, $zurueck.spo2Limit, $zurueck.volAlarm, $zurueck.pal.heart,
  $(if ($ok3) { 'OK' } else { 'NICHT WIEDERHERGESTELLT' }))
