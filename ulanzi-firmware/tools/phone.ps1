<#
  phone.ps1 - zeigt die Oberflaeche in echten Telefonbreiten nebeneinander.

  Warum der Umweg ueber Rahmen (iframe): Chrome im Headless-Modus macht das
  Fenster nie schmaler als 500 px. Ein --window-size=320 wird stillschweigend
  auf 500 gesetzt und das Bild danach nur zugeschnitten - es sieht dann so
  aus, als liefe die Seite ueber, obwohl sie nur beschnitten ist. Ein Rahmen
  von 320 px Breite in einem breiten Fenster hat dagegen wirklich einen
  320-px-Anzeigebereich, samt greifender Media Queries.

  Immer als Datei starten (-File).
#>
param(
  [string]$Ip     = '192.168.100.13',
  [int[]] $Breite = @(320, 375, 430),
  [int]   $Hoehe  = 420,
  [string]$Pfad   = '/'
)
$ErrorActionPreference = 'Stop'
$wurzel = "$PSScriptRoot\.."

$rahmen = ''
foreach ($b in $Breite) {
  $rahmen += ('<figure><figcaption>' + $b + ' px</figcaption>' +
              '<iframe src="http://' + $Ip + $Pfad + '" width="' + $b + '" height="' + $Hoehe + '"></iframe></figure>')
}

$seite = @"
<!doctype html><meta charset=utf-8>
<style>
 body{margin:0;background:#2b2f36;font:12px system-ui;display:flex;gap:14px;padding:14px;align-items:flex-start}
 figure{margin:0}
 figcaption{color:#cbd3df;padding:0 0 5px 2px}
 iframe{border:1px solid #5a6274;border-radius:8px;background:#0c0e12;display:block}
</style>
$rahmen
"@

$lokal = Join-Path $wurzel 'phone.html'
[System.IO.File]::WriteAllText($lokal, $seite, (New-Object System.Text.UTF8Encoding($false)))

$gesamt = 28 + ($Breite | Measure-Object -Sum).Sum + (14 * $Breite.Count)
$bild   = Join-Path $wurzel 'phone.png'

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $wurzel 'tools\shot.ps1') `
    -Url ('file:///' + ($lokal -replace '\\', '/')) -Width $gesamt -Height ($Hoehe + 60) `
    -Scale 1 -Out $bild 2>$null | Out-Null

if (Test-Path $bild) { Write-Output ("{0} ({1} Bytes), Breiten: {2}" -f $bild, (Get-Item $bild).Length, ($Breite -join ', ')) }
else { Write-Output 'Kein Bild entstanden.' }
