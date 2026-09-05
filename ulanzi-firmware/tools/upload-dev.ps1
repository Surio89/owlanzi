# Nur das Entwicklerboard an COM5. Die TC001 wird hier NICHT angefasst.
Set-Location "$PSScriptRoot\.."
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run -e esp32dev -t upload --upload-port COM5 2>&1 | Select-Object -Last 14
