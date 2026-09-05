# Prueft: kein BOM, gueltiges UTF-8, und zeigt ein paar Beispielzeilen.
$p = "$PSScriptRoot\..\src\webui.cpp"
$b = [System.IO.File]::ReadAllBytes($p)

Write-Output ("Groesse: {0} Bytes" -f $b.Length)
if ($b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) {
  Write-Output 'BOM: JA  <-- unerwuenscht'
} else {
  Write-Output 'BOM: nein (gut)'
}

# Strenges UTF-8: wirft bei ungueltigen Folgen.
$strict = New-Object System.Text.UTF8Encoding($false, $true)
try   { $null = $strict.GetString($b); Write-Output 'UTF-8: gueltig' }
catch { Write-Output ('UTF-8: UNGUELTIG - ' + $_.Exception.Message) }

$nonAscii = ($b | Where-Object { $_ -gt 127 }).Count
Write-Output ("Bytes > 127: {0}" -f $nonAscii)

Write-Output ''
Write-Output 'Beispielzeilen:'
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)
$i = 0
foreach ($line in ($t -split "`r?`n")) {
  $i++
  if ($line -match '[\u00c4\u00d6\u00dc\u00e4\u00f6\u00fc\u00df]') {
    Write-Output ("{0,5}: {1}" -f $i, $line.Trim())
  }
}
