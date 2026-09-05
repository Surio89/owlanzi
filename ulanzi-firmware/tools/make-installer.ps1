<#
  make-installer.ps1 - baut das eine Abbild fuer den Web-Installer:
  Bootloader, Partitionstabelle, OTA-Marker und Programm in einer Datei ab
  0x0. Das ist es, was ESP Web Tools im Browser auf die TC001 schreibt.

  Gebaut wird aus der Umgebung "release", NICHT aus "ulanzi". Der Unterschied
  ist -DNO_LOCAL_SECRETS: ohne diesen Schalter zieht main.cpp die lokale
  secrets_local.h mit ins Abbild, und dann traegt jede weitergegebene .bin
  das WLAN- und das Owlet-Passwort des Entwicklers im Klartext mit sich.
  Genau das ist hier einmal passiert - deshalb prueft das Skript zum Schluss
  das fertige Abbild und bricht ab, wenn es doch etwas findet.

  Immer als Datei starten (-File).
#>
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'

$root  = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root '.pio\build\release'
$out   = Join-Path $root 'web-installer\owlanzi-tc001.bin'
$pio   = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'

if (-not $SkipBuild) {
  Write-Output 'Baue Umgebung "release" (ohne secrets_local.h) ...'
  # Der Compiler schreibt Warnungen auf den Fehlerkanal; bei
  # ErrorActionPreference=Stop wuerde PowerShell das als Abbruch werten.
  $ErrorActionPreference = 'Continue'
  & $pio run -e release -d $root 2>&1 | Select-Object -Last 6
  $code = $LASTEXITCODE
  $ErrorActionPreference = 'Stop'
  if ($code -ne 0) { throw 'Bau fehlgeschlagen.' }
}

$py      = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'

foreach ($f in @('bootloader.bin', 'partitions.bin', 'firmware.bin')) {
  $p = Join-Path $build $f
  if (-not (Test-Path $p)) { throw "fehlt: $p" }
}
$boot = Join-Path $build 'bootloader.bin'
$part = Join-Path $build 'partitions.bin'
$app  = Join-Path $build 'firmware.bin'
$ota  = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'

$ErrorActionPreference = 'Continue'
& $py $esptool --chip esp32 merge_bin -o $out `
   --flash_mode dio --flash_freq 40m --flash_size 8MB `
   0x1000 $boot 0x8000 $part 0xe000 $ota 0x10000 $app
$code = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($code -ne 0) { throw 'merge_bin fehlgeschlagen.' }

# --- Riegel: keine Zugangsdaten im Abbild ---------------------------------
# Die Suchbegriffe kommen aus secrets_local.h, damit sie nirgends sonst
# stehen muessen. Fehlt die Datei, ist ohnehin nichts zu finden.
$geheim = Join-Path $root 'src\secrets_local.h'
if (Test-Path $geheim) {
  $nadeln = @()
  foreach ($z in (Get-Content $geheim)) {
    $m = [regex]::Match($z, '#define\s+\w+\s+"([^"]+)"')
    if ($m.Success -and $m.Groups[1].Value.Length -ge 8) { $nadeln += $m.Groups[1].Value }
  }
  $bytes = [System.IO.File]::ReadAllBytes($out)
  $utf8  = New-Object System.Text.UTF8Encoding($false)
  # Bewusst nur ASCII-Namen: Windows PowerShell liest .ps1 ohne BOM als ANSI,
  # ein Umlaut im Variablennamen zerlegt das Skript still.
  $treffer = @()
  foreach ($n in $nadeln) {
    $nb = $utf8.GetBytes($n); $gefunden = $false
    for ($i = 0; $i -le $bytes.Length - $nb.Length; $i++) {
      $ok = $true
      for ($j = 0; $j -lt $nb.Length; $j++) { if ($bytes[$i + $j] -ne $nb[$j]) { $ok = $false; break } }
      if ($ok) { $gefunden = $true; break }
    }
    if ($gefunden) { $treffer += ('ein Begriff mit ' + $n.Length + ' Zeichen') }
  }
  if ($treffer.Count) {
    Remove-Item $out -Force
    throw ("ABBRUCH: das Abbild enthaelt Zugangsdaten (" + ($treffer -join ', ') +
           "). Es wurde geloescht. Wurde wirklich die Umgebung 'release' gebaut?")
  }
  Write-Output ("Riegel: {0} Suchbegriffe geprueft, keiner im Abbild." -f $nadeln.Count)
} else {
  Write-Output 'Riegel: keine secrets_local.h vorhanden, nichts zu pruefen.'
}

$i = Get-Item $out
Write-Output ('{0}  {1:N0} Bytes  {2}' -f $i.Name, $i.Length, $i.LastWriteTime)
