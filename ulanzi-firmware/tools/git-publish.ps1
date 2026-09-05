<#
  git-publish.ps1 - legt das oeffentliche Repo an und schiebt den Commit hoch.

  Voraussetzung: du hast dich einmal angemeldet:

      gh auth login

  Das machst du selbst - eine Anmeldung ist nichts, was jemand anders fuer
  dich erledigt. Danach reicht dieses Skript.

  Immer als Datei starten (-File).
#>
param(
  [string]$Name       = 'owlanzi',
  [string]$Beschreibung = 'Eigene Firmware fuer die Ulanzi TC001: zeigt die Werte des Owlet Smart Sock direkt auf der Pixelmatrix - ohne Home Assistant.'
)
$ErrorActionPreference = 'Continue'

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $root

$gh = 'C:\Program Files\GitHub CLI\gh.exe'
if (-not (Test-Path $gh)) { $gh = 'gh' }

Write-Output '--- Anmeldung ---'
& $gh auth status 2>&1 | Out-String | Write-Output
Write-Output ''

Write-Output '--- Was hochgeht ---'
git log --oneline -1
Write-Output ('Dateien im Commit: ' + (git show --name-only --pretty=format: HEAD | Where-Object { $_ } | Measure-Object).Count)
Write-Output ''

Write-Output ("--- Lege oeffentliches Repo '{0}' an und schiebe hoch ---" -f $Name)
& $gh repo create $Name --public --source . --remote origin --push --description $Beschreibung 2>&1 | Out-String | Write-Output

Write-Output ''
Write-Output '--- Ergebnis ---'
git remote -v
& $gh repo view --json url,visibility,name 2>&1 | Out-String | Write-Output
