$ErrorActionPreference = 'Continue'
$browser = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
if (-not (Test-Path $browser)) { $browser = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' }
Write-Output ('Browser: ' + $browser)
$lokal  = "$PSScriptRoot\..\measure.html"
Write-Output ('measure.html vorhanden: ' + (Test-Path $lokal) + '  Bytes: ' + (Get-Item $lokal).Length)
$profil = Join-Path $env:TEMP ('probe-' + [guid]::NewGuid().ToString('N'))
$dom = & $browser '--headless=new' '--disable-gpu' "--user-data-dir=$profil" '--window-size=320,800' '--virtual-time-budget=6000' '--dump-dom' $lokal 2>$null
Remove-Item $profil -Recurse -Force -ErrorAction SilentlyContinue
$text = $dom -join "`n"
Write-Output ('DOM-Laenge: ' + $text.Length)
$t = [regex]::Match($text, '<title>(.*?)</title>', 'Singleline')
Write-Output ('title: ' + $(if ($t.Success) { $t.Groups[1].Value } else { '(kein title-Element im Auszug)' }))
Write-Output '--- erste 300 Zeichen ---'
Write-Output ($text.Substring(0, [Math]::Min(300, $text.Length)))
