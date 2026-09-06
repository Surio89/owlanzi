<#
  find-german.ps1 - lists lines that still look German.

  The repository is English; the only German that belongs in it is the DE
  translation table in webui.cpp and the German half of every L("en","de")
  pair. Everything else - comments, log lines, error strings - should read
  English. This is a checklist, not a judge: look at every hit.

  Always run as a file (-File).
#>
param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path $PSScriptRoot -Parent }

# Words that are common in German and rare or absent in English prose.
$woerter = @('der','die','das','und','nicht','wird','wenn','dann','damit','ist',
             'sind','eine','einen','einem','kein','keine','muss','kann','aber',
             'auch','nur','schon','noch','dass','fuer','ueber','ohne','vom',
             'zum','zur','beim','dem','den','des','sich','hier','dort','sonst',
             'statt','beim','laeuft','waere','koennte','soll','sollte','gibt',
             'macht','geht','steht','liegt','bleibt','wurde','haben','hat')
$muster = '(?i)\b(' + ($woerter -join '|') + ')\b'

$dateien = Get-ChildItem -Path $Root -Recurse -File -Include *.cpp, *.h, *.ps1, *.js, *.md, *.ini, *.json, *.html |
  Where-Object { $_.FullName -notmatch '\\\.pio\\|\\\.git\\|\\backup\\|\\scratch\\' }

$gesamt = 0
foreach ($f in $dateien) {
  $treffer = @()
  $n = 0
  foreach ($z in [System.IO.File]::ReadAllLines($f.FullName)) {
    $n++
    if ($z -match $muster) { $treffer += ('    ' + $n.ToString().PadLeft(4) + '  ' + $z.Trim()) }
  }
  if ($treffer.Count) {
    $gesamt += $treffer.Count
    Write-Output ('--- ' + $f.FullName.Substring($Root.Length + 1) + '  (' + $treffer.Count + ') ---')
    $treffer | ForEach-Object { Write-Output $_ }
  }
}
Write-Output ''
Write-Output ("Lines flagged: {0}" -f $gesamt)
