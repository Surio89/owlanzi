<#
  scan-secrets.ps1 - looks for credentials in the project before anything is
  made public.

  The search terms are NOT written into this script; they are read out of
  src\secrets_local.h. That file is in .gitignore - a script with the
  passwords baked in would be exactly the leak it is meant to prevent.

  Everything is searched except .pio, .git, .vscode and backup.
  Always run as a file (-File).
#>
param(
  [string]$Root    = '',
  [string]$Secrets = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Root)    { $Root = Split-Path $PSScriptRoot -Parent }
if (-not $Secrets) { $Secrets = Join-Path $Root 'src\secrets_local.h' }

if (-not (Test-Path $Secrets)) { Write-Output 'No secrets_local.h found - nothing to compare against.'; exit 0 }

# Pull the values out of the #define lines.
$needles = @()
foreach ($line in (Get-Content $Secrets)) {
  $m = [regex]::Match($line, '#define\s+\w+\s+"([^"]+)"')
  if ($m.Success -and $m.Groups[1].Value.Length -ge 4) { $needles += $m.Groups[1].Value }
}
$needles = $needles | Sort-Object -Unique -CaseSensitive
Write-Output ("Search terms from secrets_local.h: {0} (not printed here)" -f $needles.Count)

$skip = @('\.pio\', '\.git\', '\.vscode\', '\backup\', 'secrets_local.h')
$files = Get-ChildItem -Path $Root -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $p = $_.FullName; -not ($skip | Where-Object { $p -like ('*' + $_ + '*') }) }

Write-Output ("Files searched: {0}" -f $files.Count)

$hits = @()
foreach ($f in $files) {
  $text = $null
  try { $text = [System.IO.File]::ReadAllText($f.FullName) } catch { continue }
  if (-not $text) { continue }
  foreach ($n in $needles) {
    if ($text.Contains($n)) {
      $hits += [pscustomobject]@{ File = $f.FullName.Substring($Root.Length + 1); Length = $n.Length }
    }
  }
}

if ($hits.Count) {
  Write-Output ''
  Write-Output 'FOUND - these files contain credentials in plain text:'
  $hits | Sort-Object File | ForEach-Object { Write-Output ('  ' + $_.File + '   (term of ' + $_.Length + ' characters)') }
} else {
  Write-Output ''
  Write-Output 'Clean: no search term found outside secrets_local.h.'
}

# The build outputs live in .pio and are excluded above - but they are
# precisely the dangerous case: the strings from secrets_local.h end up
# inside the image while compiling. Here is the direct comparison.
Write-Output ''
Write-Output 'Build outputs (this is where -DNO_LOCAL_SECRETS shows its work):'
foreach ($b in (Get-ChildItem -Path (Join-Path $Root '.pio\build') -Recurse -Filter 'firmware.bin' -ErrorAction SilentlyContinue)) {
  $raw  = [System.IO.File]::ReadAllBytes($b.FullName)
  $utf8 = New-Object System.Text.UTF8Encoding($false)
  $found = 0
  foreach ($n in $needles) {
    if ($n.Length -lt 8) { continue }   # short terms collide with the brand name
    $nb = $utf8.GetBytes($n)
    for ($i = 0; $i -le $raw.Length - $nb.Length; $i++) {
      $ok = $true
      for ($j = 0; $j -lt $nb.Length; $j++) { if ($raw[$i + $j] -ne $nb[$j]) { $ok = $false; break } }
      if ($ok) { $found++; break }
    }
  }
  $where = $b.FullName.Substring($Root.Length + 1)
  if ($found) { Write-Output ("  DANGER  {0}: {1} credentials inside the image" -f $where, $found) }
  else        { Write-Output ("  clean   {0}" -f $where) }
}

# Images can show credentials without containing them as text.
Write-Output ''
Write-Output 'Images in the project (judge each one - a screenshot can show an email address):'
$files | Where-Object { $_.Extension -match '^\.(png|jpg|jpeg|gif|webp)$' } |
  ForEach-Object { Write-Output ('  ' + $_.FullName.Substring($Root.Length + 1)) }
