<#
  console.ps1 - laedt die Seite im Hintergrundbrowser und gibt aus, was in
  der Entwicklerkonsole landet. Damit lassen sich Laufzeitfehler finden, die
  ein Bildschirmfoto nur als "alles schwarz" zeigt.

  Immer als Datei starten (-File).
#>
param(
  [string]$Url = 'http://192.168.100.13/#display'
)
$ErrorActionPreference = 'Continue'
$browser = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
if (-not (Test-Path $browser)) { $browser = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' }
$profil = Join-Path $env:TEMP ('owlanzi-log-' + [guid]::NewGuid().ToString('N'))
$logd   = Join-Path $env:TEMP ('owlanzi-log-out-' + [guid]::NewGuid().ToString('N') + '.txt')

$args = @(
  '--headless=new','--disable-gpu',"--user-data-dir=$profil",
  '--window-size=900,1200','--virtual-time-budget=6000',
  '--enable-logging=stderr','--v=1',
  "--screenshot=$env:TEMP\owlanzi-log.png", $Url
)
& $browser @args 2>&1 | Out-File -FilePath $logd -Encoding utf8
Remove-Item $profil -Recurse -Force -ErrorAction SilentlyContinue

$zeilen = Get-Content $logd -ErrorAction SilentlyContinue
$treffer = $zeilen | Where-Object { $_ -match 'CONSOLE|ERROR|Uncaught|SEVERE' }
if ($treffer) {
  Write-Output '--- Meldungen ---'
  $treffer | Select-Object -First 40 | ForEach-Object { Write-Output ('  ' + $_) }
} else {
  Write-Output 'Keine Konsolenmeldungen gefunden.'
  Write-Output ('Protokollzeilen gesamt: ' + $zeilen.Count)
}
