# Shows the actual Unicode code points of the non-ASCII characters in the
# German translation table. Needed because some display paths render umlauts
# in upper case, which once hid a real bug for an hour.
param([string]$Path = "$PSScriptRoot\..\src\webui.cpp")
$t = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)

$counts = @{}
foreach ($ch in $t.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output 'code point  count'
foreach ($k in ($counts.Keys | Sort-Object)) {
  Write-Output ('{0}     {1}' -f $k, $counts[$k])
}

Write-Output ''
Write-Output 'Expected: U+00E4 ae, U+00F6 oe, U+00FC ue, U+00C4 Ae, U+00D6 Oe, U+00DC Ue, U+00DF ss'
