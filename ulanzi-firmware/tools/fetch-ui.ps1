# Holt die ausgelieferte Seite und prueft sie so, wie der Browser sie sieht.
param([string]$Ip = '192.168.100.13')

$url = "http://$Ip/"
try {
  $r = Invoke-WebRequest -Uri $url -TimeoutSec 15 -UseBasicParsing
} catch {
  Write-Output ('Abruf fehlgeschlagen: ' + $_.Exception.Message); exit 1
}

Write-Output ('HTTP ' + [int]$r.StatusCode + '  Content-Type: ' + $r.Headers['Content-Type'])
Write-Output ('Bytes: ' + $r.RawContentLength)

# Der Server schickt UTF-8; RawContentStream enthaelt die Rohbytes.
$ms = New-Object System.IO.MemoryStream
$r.RawContentStream.Position = 0
$r.RawContentStream.CopyTo($ms)
$bytes = $ms.ToArray()
$html  = (New-Object System.Text.UTF8Encoding($false)).GetString($bytes)

$out = "$PSScriptRoot\..\served.html"
[System.IO.File]::WriteAllText($out, $html, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("gespeichert: $out")

$ae=[char]0xE4; $oe=[char]0xF6; $ue=[char]0xFC; $aeU=[char]0xC4; $sz=[char]0xDF
$proben = @(
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
foreach ($p in $proben) {
  $zeig = ($p.ToCharArray() | ForEach-Object {
    if ([int]$_ -gt 127) { '<U+{0:X4}>' -f [int]$_ } else { $_ } }) -join ''
  if ($html.Contains($p)) { Write-Output ("  OK     " + $zeig) }
  else                    { Write-Output ("  FEHLT  " + $zeig) }
}

# Zaehlung der Sonderzeichen in der gelieferten Seite.
$counts = @{}
foreach ($ch in $html.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output ''
Write-Output 'Sonderzeichen in der Seite:'
foreach ($k in ($counts.Keys | Sort-Object)) { Write-Output ('  {0} x{1}' -f $k, $counts[$k]) }

# Ein Ersatzzeichen U+FFFD hiesse: die Bytes sind kein sauberes UTF-8.
if ($html.Contains([char]0xFFFD)) { Write-Output 'WARNUNG: Ersatzzeichen gefunden - Kodierung kaputt.' }
else { Write-Output 'Kein Ersatzzeichen - Kodierung sauber.' }
