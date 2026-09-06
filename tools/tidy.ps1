# Moves the working files out of the project root into tools\scratch.
# Nothing is deleted - whatever is still needed sits there.
$root = Split-Path $PSScriptRoot -Parent
$dst  = Join-Path $root 'tools\scratch'
New-Item -ItemType Directory -Force -Path $dst | Out-Null

# Fixed names and patterns mixed; patterns only bite in the root, not in src\.
$patterns = @('served.html', 'served.js', 'served-setup.html', 'audit-setup.txt',
              'bootlog.txt', 'monitor.log', 'monitor2.log',
              'measure.html', 'measure.png', 'phone.html', 'phone.png',
              'tab.html', 'checkbin.txt',
              'shot*.png', 'w*.png', 'tab*.png', 'disp*.png')

foreach ($m in $patterns) {
  foreach ($f in (Get-ChildItem -Path $root -Filter $m -File -ErrorAction SilentlyContinue)) {
    Move-Item -LiteralPath $f.FullName -Destination (Join-Path $dst $f.Name) -Force
    Write-Output ('moved: ' + $f.Name)
  }
}

# The backup sits next to the source, not in the root.
$bak = Join-Path $root 'src\webui.cpp.bak'
if (Test-Path $bak) {
  Move-Item -LiteralPath $bak -Destination (Join-Path $dst 'webui.cpp.bak') -Force
  Write-Output 'moved: src\webui.cpp.bak'
}

Write-Output ''
Write-Output 'Project root:'
Get-ChildItem $root -File | ForEach-Object { Write-Output ('  ' + $_.Name) }
