<#
  console.ps1 - loads the page in a headless browser and prints what lands in
  the developer console. This is how you find runtime errors that a
  screenshot only shows as "everything is black".

  Always run as a file (-File).
#>
param([string]$Url = 'http://192.168.100.13/#display')
$ErrorActionPreference = 'Continue'
$browser = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
if (-not (Test-Path $browser)) { $browser = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' }
$profileDir = Join-Path $env:TEMP ('owlanzi-log-' + [guid]::NewGuid().ToString('N'))
$logFile    = Join-Path $env:TEMP ('owlanzi-log-out-' + [guid]::NewGuid().ToString('N') + '.txt')

$argList = @(
  '--headless=new','--disable-gpu',"--user-data-dir=$profileDir",
  '--window-size=900,1200','--virtual-time-budget=6000',
  '--enable-logging=stderr','--v=1',
  "--screenshot=$env:TEMP\owlanzi-log.png", $Url
)
& $browser @argList 2>&1 | Out-File -FilePath $logFile -Encoding utf8
Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue

$lines = Get-Content $logFile -ErrorAction SilentlyContinue
$hits  = $lines | Where-Object { $_ -match 'CONSOLE|ERROR|Uncaught|SEVERE' }
if ($hits) {
  Write-Output '--- messages ---'
  $hits | Select-Object -First 40 | ForEach-Object { Write-Output ('  ' + $_) }
} else {
  Write-Output 'No console messages found.'
  Write-Output ('log lines total: ' + $lines.Count)
}
