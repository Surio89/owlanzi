# Checks in the finished binary image that the web interface really sits in
# there as UTF-8. The ESP serves these bytes verbatim, so this is the most
# honest check available without the device on the network.
#
# POWERSHELL TRAP: in an array literal the comma binds TIGHTER than the plus.
# 'a' + $x + 'b', 'c' becomes one single concatenated string. Every element
# therefore needs its own parentheses.
param([string]$bin = "$PSScriptRoot\..\.pio\build\esp32dev\firmware.bin")
$b = [System.IO.File]::ReadAllBytes($bin)

function Find-Bytes([byte[]]$hay, [byte[]]$needle) {
  $n = $needle.Length
  for ($i = 0; $i -le $hay.Length - $n; $i++) {
    $ok = $true
    for ($j = 0; $j -lt $n; $j++) { if ($hay[$i + $j] -ne $needle[$j]) { $ok = $false; break } }
    if ($ok) { return $i }
  }
  return -1
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
# Built from code points so this file itself stays pure ASCII - Windows
# PowerShell reads a .ps1 without a BOM as ANSI, and a literal umlaut here
# would arrive mangled and the probe would look for the wrong bytes.
$ae=[char]0xE4; $oe=[char]0xF6; $ue=[char]0xFC; $aeU=[char]0xC4; $sz=[char]0xDF

# German words from the translation table, plus a few structural markers.
$probes = @(
  ('Oberfl' + $ae + 'che'),
  ('Ger' + $ae + 't'),
  ('pr' + $ue + 'ft'),
  ('l' + $ae + 'uft'),
  ('l' + $oe + 'schen'),
  ('hei' + $sz + 't'),
  ($aeU + 'nderung'),
  ('data-i18n-ph=phKeep'),
  ("phKeep:'leer lassen = beibehalten'"),
  ('leave empty to keep'),
  ('charset=utf-8'),
  ('data-i18n-ph'),
  ('class=logo role=img aria-label=owlanzi'),
  ('<g fill="#59E6CF">'),
  ('<g id="wordmark"'),
  ('<rect id="i-dot"'),
  ('<select id=pollSeconds>'),
  ('data-pv=vitals'),
  ('id=pvVit'),
  ('<input type=checkbox id=simBri>')
)

foreach ($p in $probes) {
  $pos = Find-Bytes $b $utf8.GetBytes($p)
  $show = ($p.ToCharArray() | ForEach-Object {
    if ([int]$_ -gt 127) { '<U+{0:X4}>' -f [int]$_ } else { $_ } }) -join ''
  if ($pos -ge 0) { Write-Output ("  OK      {0}" -f $show) }
  else            { Write-Output ("  MISSING {0}" -f $show) }
}
