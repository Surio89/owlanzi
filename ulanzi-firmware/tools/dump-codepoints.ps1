# Zeigt die tatsaechlichen Unicode-Codepunkte der Sonderzeichen.
# Noetig, weil manche Anzeigepfade Umlaute beim Darstellen gross schreiben.
$p = "$PSScriptRoot\..\src\webui.cpp"
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)

$counts = @{}
foreach ($ch in $t.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output 'Codepunkt  Anzahl'
foreach ($k in ($counts.Keys | Sort-Object)) {
  Write-Output ('{0}     {1}' -f $k, $counts[$k])
}

Write-Output ''
Write-Output 'Erwartet: U+00E4 ae, U+00F6 oe, U+00FC ue, U+00C4 Ae, U+00D6 Oe, U+00DC Ue, U+00DF ss'
Write-Output ''
Write-Output 'Kontrolle - Codepunkte in vier Beispielwoertern:'
foreach ($w in @('Oberfl','FarbA','Token l','ZurA','ZurU','hei','wei')) {
  $i = $t.IndexOf($w)
  if ($i -ge 0) {
    $seg = $t.Substring($i, [Math]::Min(16, $t.Length - $i))
    $cp  = ($seg.ToCharArray() | ForEach-Object { if ([int]$_ -gt 127) { 'U+{0:X4}' -f [int]$_ } else { $_ } }) -join ''
    Write-Output ("  {0,-9} -> {1}" -f $w, $cp)
  }
}
