# Fetches the setup page (/setup) and checks its encoding.
param([string]$Ip = '192.168.100.13')
$r = Invoke-WebRequest -Uri "http://$Ip/setup" -TimeoutSec 15 -UseBasicParsing
Write-Output ('HTTP ' + [int]$r.StatusCode + '  ' + $r.Headers['Content-Type'] + '  ' + $r.RawContentLength + ' bytes')

$ms = New-Object System.IO.MemoryStream
$r.RawContentStream.Position = 0
$r.RawContentStream.CopyTo($ms)
$html = (New-Object System.Text.UTF8Encoding($false)).GetString($ms.ToArray())
$out = "$PSScriptRoot\..\served-setup.html"
[System.IO.File]::WriteAllText($out, $html, (New-Object System.Text.UTF8Encoding($false)))
Write-Output "saved: $out"

$counts = @{}
foreach ($ch in $html.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output 'Non-ASCII characters:'
foreach ($k in ($counts.Keys | Sort-Object)) { Write-Output ('  {0} x{1}' -f $k, $counts[$k]) }
if ($html.Contains([char]0xFFFD)) { Write-Output 'WARNING: replacement character - encoding is broken.' }
else { Write-Output 'No replacement character - encoding is clean.' }
