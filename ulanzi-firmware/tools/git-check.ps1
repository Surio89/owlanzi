# Prueft, was fuer eine Veroeffentlichung schon da ist.
$ErrorActionPreference = 'Continue'
$root = "$PSScriptRoot\.."

function Zeig([string]$was, [scriptblock]$tu) {
  try { $r = & $tu 2>&1 | Out-String; Write-Output ("--- {0} ---`n{1}" -f $was, $r.Trim()) }
  catch { Write-Output ("--- {0} ---`nnicht vorhanden: {1}" -f $was, $_.Exception.Message) }
}

Zeig 'git --version'   { git --version }
Zeig 'gh --version'    { gh --version }
Zeig 'gh auth status'  { gh auth status }
Zeig 'git repo hier'   { Set-Location $root; git rev-parse --show-toplevel }
Zeig 'git config user' { git config --global user.name; git config --global user.email }
Write-Output ("--- .git im Projektstamm ---`n" + (Test-Path (Join-Path $root '.git')))
Write-Output ("--- .git im Website-Ordner ---`n" + (Test-Path (Join-Path $root 'owlanzi-website\.git')))
