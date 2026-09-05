# Prueft im fertigen Binaerabbild, dass die Weboberflaeche wirklich als
# UTF-8 drinsteht. Der ESP liefert diese Bytes 1:1 aus, also ist das die
# ehrlichste Kontrolle ohne das Geraet im Netz zu haben.
#
# ACHTUNG PowerShell: im Array-Literal bindet das Komma STAERKER als das
# Plus. 'a' + $x + 'b', 'c' wird zu einem einzigen String verkettet.
# Jedes Element braucht darum eigene Klammern.
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
$ae=[char]0xE4; $oe=[char]0xF6; $ue=[char]0xFC; $aeU=[char]0xC4; $sz=[char]0xDF

$proben = @(
  ('Oberfl' + $ae + 'che'),
  ('Ger' + $ae + 't'),
  ('pr' + $ue + 'ft'),
  ('l' + $ae + 'uft'),
  ('l' + $oe + 'schen'),
  ('hei' + $sz + 'en'),
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

foreach ($p in $proben) {
  $pos = Find-Bytes $b $utf8.GetBytes($p)
  $zeigen = ($p.ToCharArray() | ForEach-Object {
    if ([int]$_ -gt 127) { '<U+{0:X4}>' -f [int]$_ } else { $_ } }) -join ''
  if ($pos -ge 0) { Write-Output ("  OK     {0}" -f $zeigen) }
  else            { Write-Output ("  FEHLT  {0}" -f $zeigen) }
}
