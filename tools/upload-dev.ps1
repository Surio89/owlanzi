# The dev board on COM5 only. The TC001 is NOT touched here.
Set-Location "$PSScriptRoot\.."
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run -e esp32dev -t upload --upload-port COM5 2>&1 | Select-Object -Last 14
