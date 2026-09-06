# Checks: no BOM, valid UTF-8, and lists the lines carrying umlauts.
# The German translation table lives inside webui.cpp, so the encoding of
# that one source file decides whether the interface reads correctly.
param([string]$Path = "$PSScriptRoot\..\src\webui.cpp")
$b = [System.IO.File]::ReadAllBytes($Path)

Write-Output ("size: {0} bytes" -f $b.Length)
if ($b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) {
  Write-Output 'BOM: YES  <-- not wanted'
} else {
  Write-Output 'BOM: no (good)'
}

# Strict UTF-8: throws on invalid sequences.
$strict = New-Object System.Text.UTF8Encoding($false, $true)
try   { $null = $strict.GetString($b); Write-Output 'UTF-8: valid' }
catch { Write-Output ('UTF-8: INVALID - ' + $_.Exception.Message) }

$nonAscii = ($b | Where-Object { $_ -gt 127 }).Count
Write-Output ("bytes > 127: {0}" -f $nonAscii)

Write-Output ''
Write-Output 'Lines with non-ASCII characters:'
# This file stays pure ASCII on purpose: Windows PowerShell reads a .ps1
# without a BOM as ANSI, and a literal umlaut written here would arrive
# mangled - the test would then quietly match nothing. So the check asks for
# code points instead of spelling the characters out.
$t = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
$i = 0
foreach ($line in ($t -split "`r?`n")) {
  $i++
  $hit = $false
  foreach ($ch in $line.ToCharArray()) { if ([int]$ch -gt 127) { $hit = $true; break } }
  if ($hit) { Write-Output ("{0,5}: {1}" -f $i, $line.Trim()) }
}
