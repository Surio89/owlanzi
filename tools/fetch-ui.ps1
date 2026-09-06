# Fetches the served page and checks it the way a browser sees it.
param([string]$Ip = '192.168.100.13')
$root = Split-Path $PSScriptRoot -Parent

$url = "http://$Ip/"
try {
  $r = Invoke-WebRequest -Uri $url -TimeoutSec 15 -UseBasicParsing
} catch {
  Write-Output ('fetch failed: ' + $_.Exception.Message); exit 1
}

Write-Output ('HTTP ' + [int]$r.StatusCode + '  Content-Type: ' + $r.Headers['Content-Type'])
Write-Output ('bytes: ' + $r.RawContentLength)

# The server sends UTF-8; RawContentStream holds the raw bytes.
$ms = New-Object System.IO.MemoryStream
$r.RawContentStream.Position = 0
$r.RawContentStream.CopyTo($ms)
$bytes = $ms.ToArray()
$html  = (New-Object System.Text.UTF8Encoding($false)).GetString($bytes)

$out = Join-Path $root 'served.html'
[System.IO.File]::WriteAllText($out, $html, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("saved: $out")

# German words from the translation table, built from code points so this
# file stays pure ASCII (see check-bin.ps1 for why that matters).
$ae=[char]0xE4; $oe=[char]0xF6; $ue=[char]0xFC; $aeU=[char]0xC4; $sz=[char]0xDF
$probes = @(
  ('Oberfl' + $ae + 'che'),
  ('Ger' + $ae + 't'),
  ('pr' + $ue + 'ft'),
  ('l' + $ae + 'uft'),
  ($aeU + 'nderung'),
  ('hei' + $sz + 'en'),
  ('data-i18n-ph=phKeep'),
  ("phKeep:'leer lassen = beibehalten'"),
  ('placeholder="leave empty to keep"'),
  ('charset=utf-8'),
  ('dataset.i18nPh')
)
Write-Output ''
foreach ($p in $probes) {
  $show = ($p.ToCharArray() | ForEach-Object {
    if ([int]$_ -gt 127) { '<U+{0:X4}>' -f [int]$_ } else { $_ } }) -join ''
  if ($html.Contains($p)) { Write-Output ("  OK      " + $show) }
  else                    { Write-Output ("  MISSING " + $show) }
}

# Count the non-ASCII characters in the delivered page.
$counts = @{}
foreach ($ch in $html.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output ''
Write-Output 'Non-ASCII characters in the page:'
foreach ($k in ($counts.Keys | Sort-Object)) { Write-Output ('  {0} x{1}' -f $k, $counts[$k]) }

# A replacement character U+FFFD would mean the bytes are not clean UTF-8.
if ($html.Contains([char]0xFFFD)) { Write-Output 'WARNING: replacement character found - encoding is broken.' }
else { Write-Output 'No replacement character - encoding is clean.' }
