# Raeumt die Zwischenergebnisse aus dem Projektstamm nach tools\scratch.
# Nichts wird geloescht - was noch gebraucht wird, liegt dort.
$root = "$PSScriptRoot\.."
$dst  = Join-Path $root 'tools\scratch'
New-Item -ItemType Directory -Force -Path $dst | Out-Null

# Feste Namen und Muster gemischt; Muster greifen nur im Stamm, nicht in src\.
$muster = @('served.html', 'served.js', 'served-setup.html', 'audit-setup.txt',
            'bootlog.txt', 'monitor.log', 'monitor2.log',
            'measure.html', 'measure.png', 'phone.html', 'phone.png',
            'tab.html', 'checkbin.txt',
            'shot*.png', 'w*.png', 'tab*.png', 'disp*.png')

foreach ($m in $muster) {
  foreach ($f in (Get-ChildItem -Path $root -Filter $m -File -ErrorAction SilentlyContinue)) {
    Move-Item -LiteralPath $f.FullName -Destination (Join-Path $dst $f.Name) -Force
    Write-Output ('verschoben: ' + $f.Name)
  }
}

# Die Sicherung liegt neben der Quelle, nicht im Stamm.
$bak = Join-Path $root 'src\webui.cpp.bak'
if (Test-Path $bak) {
  Move-Item -LiteralPath $bak -Destination (Join-Path $dst 'webui.cpp.bak') -Force
  Write-Output 'verschoben: src\webui.cpp.bak'
}

Write-Output ''
Write-Output 'Projektstamm:'
Get-ChildItem $root -File | ForEach-Object { Write-Output ('  ' + $_.Name) }
