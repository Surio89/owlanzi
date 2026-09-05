# Setzt das Entwicklerboard an COM5 zurueck und faengt den Startlauf ein.
# Nur COM5 - die TC001 wird nicht angefasst.
param([int]$Seconds = 18)
$out = "$PSScriptRoot\..\bootlog.txt"
$sp = New-Object System.IO.Ports.SerialPort 'COM5',115200,None,8,one
$sp.Open()

# Reset ueber die Auto-Reset-Leitungen: EN low, dann wieder frei.
$sp.DtrEnable = $false; $sp.RtsEnable = $true
Start-Sleep -Milliseconds 150
$sp.RtsEnable = $false
Start-Sleep -Milliseconds 100

$sb = New-Object System.Text.StringBuilder
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  try { $s = $sp.ReadExisting(); if ($s) { [void]$sb.Append($s) } } catch {}
  Start-Sleep -Milliseconds 120
}
$sp.Close()
[System.IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Output "geschrieben: $out"
