param([string]$Env = 'esp32dev')
Set-Location "$PSScriptRoot\.."
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run -e $Env 2>&1 | Select-Object -Last 28
