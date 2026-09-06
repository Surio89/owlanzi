<#
  make-installer.ps1 - builds the single image for the web installer:
  bootloader, partition table, OTA marker and program in one file from 0x0.
  That is what ESP Web Tools writes onto the TC001 from the browser.

  It builds from the "release" environment, NOT from "ulanzi". The difference
  is -DNO_LOCAL_SECRETS: without that flag main.cpp pulls the local
  secrets_local.h into the image, and every copy handed out then carries the
  developer's Wi-Fi and Owlet password in plain text. That happened here
  once - which is why this script scans the finished image at the end and
  aborts if it finds anything.

  Always run as a file (-File).
#>
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'

$root  = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root '.pio\build\release'
$out   = Join-Path $root 'web-installer\owlanzi-tc001.bin'
$pio   = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'

if (-not $SkipBuild) {
  Write-Output 'Building environment "release" (without secrets_local.h) ...'
  # The compiler writes warnings to the error channel; with
  # ErrorActionPreference=Stop PowerShell would treat that as a failure.
  $ErrorActionPreference = 'Continue'
  & $pio run -e release -d $root 2>&1 | Select-Object -Last 6
  $code = $LASTEXITCODE
  $ErrorActionPreference = 'Stop'
  if ($code -ne 0) { throw 'Build failed.' }
}

$py      = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'

foreach ($f in @('bootloader.bin', 'partitions.bin', 'firmware.bin')) {
  $p = Join-Path $build $f
  if (-not (Test-Path $p)) { throw "missing: $p" }
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
if ($code -ne 0) { throw 'merge_bin failed.' }

# --- Guard: no credentials in the image -----------------------------------
# The search terms come from secrets_local.h so they need not be written
# anywhere else. No such file, nothing to find.
$secrets = Join-Path $root 'src\secrets_local.h'
if (Test-Path $secrets) {
  $needles = @()
  foreach ($line in (Get-Content $secrets)) {
    $m = [regex]::Match($line, '#define\s+\w+\s+"([^"]+)"')
    if ($m.Success -and $m.Groups[1].Value.Length -ge 8) { $needles += $m.Groups[1].Value }
  }
  $bytes = [System.IO.File]::ReadAllBytes($out)
  $utf8  = New-Object System.Text.UTF8Encoding($false)
  $hits  = @()
  foreach ($n in $needles) {
    $nb = $utf8.GetBytes($n); $found = $false
    for ($i = 0; $i -le $bytes.Length - $nb.Length; $i++) {
      $ok = $true
      for ($j = 0; $j -lt $nb.Length; $j++) { if ($bytes[$i + $j] -ne $nb[$j]) { $ok = $false; break } }
      if ($ok) { $found = $true; break }
    }
    if ($found) { $hits += ('a term of ' + $n.Length + ' characters') }
  }
  if ($hits.Count) {
    Remove-Item $out -Force
    throw ("ABORT: the image contains credentials (" + ($hits -join ', ') +
           "). It has been deleted. Was the 'release' environment really built?")
  }
  Write-Output ("Guard: {0} search terms checked, none in the image." -f $needles.Count)
} else {
  Write-Output 'Guard: no secrets_local.h present, nothing to check.'
}

$i = Get-Item $out
Write-Output ('{0}  {1:N0} bytes  {2}' -f $i.Name, $i.Length, $i.LastWriteTime)
