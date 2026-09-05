<#
  git-prepare.ps1 - legt das Repo an und zeigt, was wirklich hineinkaeme.

  Bewusst OHNE commit und OHNE push: erst wird die Liste angesehen. Ein
  oeffentliches Repo laesst sich nicht zurueckholen, und ein einziger
  vergessener Dateiname reicht.

  Immer als Datei starten (-File).
#>
$ErrorActionPreference = 'Continue'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent   # ...\owlanzi

Set-Location $root
if (-not (Test-Path (Join-Path $root '.git'))) {
  git init -b main | Out-Null
  Write-Output 'git-Repo angelegt (Zweig main).'
} else {
  Write-Output 'git-Repo war schon da.'
}

git add -A

Write-Output ''
Write-Output ('Projektstamm: ' + $root)
Write-Output ''
Write-Output '--- Dateien, die im ersten Commit laegen ---'
$liste = git diff --cached --name-only
$liste | ForEach-Object { Write-Output ('  ' + $_) }
Write-Output ''
Write-Output ('Anzahl: ' + ($liste | Measure-Object).Count)

Write-Output ''
Write-Output '--- Kontrolle: darf NICHTS davon dabei sein ---'
$verboten = @('secrets_local.h', 'tools/scratch/', '.pio/', 'backup/', 'owlanzi-website/')
foreach ($v in $verboten) {
  $treffer = $liste | Where-Object { $_ -like ('*' + $v + '*') -and $_ -notlike '*secrets_local.h.example' }
  if ($treffer) { Write-Output ('  GEFUNDEN ' + $v + ':'); $treffer | ForEach-Object { Write-Output ('     ' + $_) } }
  else          { Write-Output ('  sauber   ' + $v) }
}

Write-Output ''
Write-Output '--- Groesse ---'
$bytes = 0
foreach ($f in $liste) { $p = Join-Path $root $f; if (Test-Path $p) { $bytes += (Get-Item $p).Length } }
Write-Output ('  {0:N0} Bytes in {1} Dateien' -f $bytes, ($liste | Measure-Object).Count)
