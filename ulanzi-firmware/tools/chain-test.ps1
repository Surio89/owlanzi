# Spielt die Owlet-Anmeldekette einmal auf dem PC durch.
# Zugangsdaten werden aus src\main.cpp gelesen und NIE ausgegeben.
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot "..\src\main.cpp"
$txt = Get-Content $src -Raw

function Get-Const($name) {
  $m = [regex]::Match($txt, [regex]::Escape($name) + '\s*=\s*"([^"]*)"')
  if (-not $m.Success) { throw "Konstante $name nicht gefunden" }
  return $m.Groups[1].Value
}

$email = Get-Const "OWLET_EMAIL"
$pass  = Get-Const "OWLET_PASSWORD"
$key   = Get-Const "EU_API_KEY"
$mini  = Get-Const "EU_MINI"
$sign  = Get-Const "EU_SIGNIN"
$base  = Get-Const "EU_BASE"

Write-Output ("Konto:  " + $email)
Write-Output ("Key:    " + $key.Substring(0,12) + "... (" + $key.Length + " Zeichen)")
Write-Output ""

$hdr  = @{ "X-Android-Package"="com.owletcare.owletcare"
           "X-Android-Cert"="2A3BC26DB0B8B0792DBE28E6FFDC2598F9B12B74" }
$body = @{ email=$email; password=$pass; returnSecureToken=$true } | ConvertTo-Json
$r1 = Invoke-RestMethod -Method Post -TimeoutSec 30 -Headers $hdr -ContentType "application/json" -Uri "https://www.googleapis.com/identitytoolkit/v3/relyingparty/verifyPassword?key=$key" -Body $body
$jwt = $r1.idToken
$dots = ($jwt.ToCharArray() | Where-Object {$_ -eq '.'}).Count
Write-Output ("1/3 Firebase OK   idToken " + $jwt.Length + " Zeichen, " + $dots + " Punkte")

# --- 2. Owlet-SSO ----------------------------------------------------------
$r2 = Invoke-RestMethod -Method Get -TimeoutSec 30 -Uri $mini -Headers @{ "Authorization" = $jwt }
$miniTok = $r2.mini_token
Write-Output ("2/3 SSO OK        mini_token " + $miniTok.Length + " Zeichen")

# --- 3. Ayla ---------------------------------------------------------------
$b3 = @{ app_id=(Get-Const "EU_APP_ID"); app_secret=(Get-Const "EU_APP_SEC"); provider="owl_id"; token=$miniTok } | ConvertTo-Json
$r3 = Invoke-RestMethod -Method Post -TimeoutSec 30 -ContentType "application/json" -Headers @{Accept="application/json"} -Uri $sign -Body $b3
$at = $r3.access_token
Write-Output ("3/3 Ayla OK       access_token " + $at.Length + " Zeichen, gilt " + $r3.expires_in + " s")
Write-Output ""

$ah = @{ "Authorization" = "auth_token $at" }

# --- 4. Geraete ------------------------------------------------------------
$dev = Invoke-RestMethod -Method Get -TimeoutSec 30 -Uri "$base/devices.json" -Headers ($ah + @{Accept="application/json"})
$dsn = $dev[0].device.dsn
Write-Output ("Geraet: " + $dsn + "  (" + $dev[0].device.product_name + ")")

# --- 5. APP_ACTIVE ---------------------------------------------------------
$null = Invoke-RestMethod -Method Post -TimeoutSec 30 -ContentType "application/json" -Uri "$base/dsns/$dsn/properties/APP_ACTIVE/datapoints.json" -Headers ($ah + @{Accept="application/json"}) -Body '{"datapoint":{"metadata":{},"value":1}}'
Write-Output "APP_ACTIVE gesetzt"
Start-Sleep -Seconds 3

# --- 6. Properties ---------------------------------------------------------
$props = Invoke-RestMethod -Method Get -TimeoutSec 30 -Uri "$base/dsns/$dsn/properties.json" -Headers ($ah + @{Accept="application/json"})
$json  = $props | ConvertTo-Json -Depth 10
Write-Output ("properties.json Groesse: " + $json.Length + " Zeichen, " + $props.Count + " Eigenschaften")
Write-Output ""
$rtv = ($props | Where-Object { $_.property.name -eq "REAL_TIME_VITALS" }).property.value
if ($rtv) {
  Write-Output "REAL_TIME_VITALS roh:"
  Write-Output $rtv
} else {
  Write-Output "KEIN REAL_TIME_VITALS. Vorhandene Namen:"
  ($props.property.name | Sort-Object) -join ", "
}
Write-Output ""
Write-Output "Alarm-Properties:"
foreach($n in @("LOW_OX_ALRT","HIGH_OX_ALRT","LOW_HR_ALRT","HIGH_HR_ALRT","LOST_POWER_ALRT","SOCK_DISCON_ALRT","SOCK_OFF","LOW_BATT_ALRT","CRIT_BATT_ALRT","CRIT_OX_ALRT")){
  $v = ($props | Where-Object { $_.property.name -eq $n }).property.value
  Write-Output ("  {0,-18} {1}" -f $n, $v)
}
