<#
  measure.ps1 - misst, wie breit die Seitenteile bei einer vorgegebenen
  Fensterbreite wirklich werden. Ein Bildschirmfoto zeigt, DASS etwas
  abgeschnitten ist; hier steht, WELCHES Element schiebt.

  Weg: die ausgelieferte Seite lokal ablegen, ein <base> laesst die Abfragen
  weiter aufs Geraet zeigen, ein Messskript haengt sich ans Ende. Es misst
  die Kaesten und schreibt sie danach als Text in die Seite - das Ergebnis
  wird als Bild abgeholt. (--dump-dom liefert im neuen Headless-Modus nichts,
  Bildschirmfotos dagegen zuverlaessig.)

  Immer als Datei starten (-File).
#>
param(
  [string]$Ip    = '192.168.100.13',
  [int]   $Width = 320
)
$ErrorActionPreference = 'Stop'
$wurzel = "$PSScriptRoot\.."

$antwort = Invoke-WebRequest -Uri ("http://{0}/" -f $Ip) -TimeoutSec 15 -UseBasicParsing
$ms = New-Object System.IO.MemoryStream
$antwort.RawContentStream.Position = 0
$antwort.RawContentStream.CopyTo($ms)
$html = (New-Object System.Text.UTF8Encoding($false)).GetString($ms.ToArray())

$mess = @'
<script>
(function () {
  function box(name, sel) {
    var e = document.querySelector(sel);
    if (!e) return name + ': fehlt';
    var r = e.getBoundingClientRect();
    return name + ': ' + Math.round(r.left) + ' .. ' + Math.round(r.right) +
           '  (breit ' + Math.round(r.width) + ')';
  }
  var z = [];
  z.push('Fenster ' + innerWidth + ' px, Dokument scrollWidth ' + document.documentElement.scrollWidth);
  z.push(box('.w      ', '.w'));
  z.push(box('.title  ', '.title'));
  z.push(box('h1      ', '.title h1'));
  z.push(box('svg.logo', 'svg.logo'));
  z.push(box('.pill   ', '.pill'));
  z.push(box('.lang   ', '.lang'));
  z.push(box('canvas  ', '#mx'));
  z.push(box('nav     ', 'nav'));
  z.push(box('.big    ', '.big'));
  var max = 0, wer = '';
  document.querySelectorAll('.w *').forEach(function (e) {
    var r = e.getBoundingClientRect();
    if (r.width > 0 && r.right > max) { max = r.right; wer = e.tagName.toLowerCase() + ' ' + (e.className || e.id); }
  });
  z.push('weitester rechter Rand: ' + Math.round(max) + '  ->  ' + wer);
  document.documentElement.innerHTML =
    '<body style="margin:0;background:#fff;color:#000;font:13px/1.5 Consolas,monospace">' +
    '<pre style="white-space:pre-wrap;padding:8px">' + z.join('\n') + '</pre></body>';
})();
</script>
'@

$lokal   = Join-Path $wurzel 'measure.html'
$mitBase = $html -replace '<meta charset=utf-8>', ('<meta charset=utf-8><base href="http://' + $Ip + '/">')
[System.IO.File]::WriteAllText($lokal, $mitBase + $mess, (New-Object System.Text.UTF8Encoding($false)))

$bild = Join-Path $wurzel 'measure.png'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $wurzel 'tools\shot.ps1') `
    -Url ('file:///' + ($lokal -replace '\\', '/')) -Width $Width -Height 340 -Scale 1 `
    -Headless old -Out $bild 2>$null | Out-Null

if (Test-Path $bild) { Write-Output ("Messbild: {0} ({1} Bytes) - Fensterbreite {2}" -f $bild, (Get-Item $bild).Length, $Width) }
else { Write-Output 'Kein Messbild entstanden.' }
