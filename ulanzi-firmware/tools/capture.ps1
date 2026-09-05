param([int]$Seconds = 80, [string]$Out = "$PSScriptRoot\..\monitor2.log")
$sp = New-Object System.IO.Ports.SerialPort "COM5",115200,None,8,one
$sp.Open()
$sb = New-Object System.Text.StringBuilder
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  try { $s = $sp.ReadExisting(); if ($s) { [void]$sb.Append($s) } } catch {}
  Start-Sleep -Milliseconds 150
}
$sp.Close()
$sb.ToString() | Set-Content $Out
