<#
  phone.ps1 - shows the interface at real phone widths, side by side.

  Why the detour through iframes: headless Chrome never makes its window
  narrower than 500 px. A --window-size=320 is silently raised to 500 and the
  image is merely cropped afterwards - which looks exactly like the page
  overflowing when in fact it is only clipped. A 320 px iframe inside a wide
  window really does have a 320 px viewport, media queries included.

  Always run as a file (-File).
#>
param(
  [string]$Ip     = '192.168.100.13',
  [int[]] $Widths = @(320, 375, 430),
  [int]   $Height = 420,
  [string]$Path   = '/'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

$frames = ''
foreach ($w in $Widths) {
  $frames += ('<figure><figcaption>' + $w + ' px</figcaption>' +
              '<iframe src="http://' + $Ip + $Path + '" width="' + $w + '" height="' + $Height + '"></iframe></figure>')
}

$page = @"
<!doctype html><meta charset=utf-8>
<style>
 body{margin:0;background:#2b2f36;font:12px system-ui;display:flex;gap:14px;padding:14px;align-items:flex-start}
 figure{margin:0}
 figcaption{color:#cbd3df;padding:0 0 5px 2px}
 iframe{border:1px solid #5a6274;border-radius:8px;background:#0c0e12;display:block}
</style>
$frames
"@

$local = Join-Path $root 'phone.html'
[System.IO.File]::WriteAllText($local, $page, (New-Object System.Text.UTF8Encoding($false)))

$total = 28 + ($Widths | Measure-Object -Sum).Sum + (14 * $Widths.Count)
$img   = Join-Path $root 'phone.png'

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tools\shot.ps1') `
    -Url ('file:///' + ($local -replace '\\', '/')) -Width $total -Height ($Height + 60) `
    -Scale 1 -Out $img 2>$null | Out-Null

if (Test-Path $img) { Write-Output ("{0} ({1} bytes), widths: {2}" -f $img, (Get-Item $img).Length, ($Widths -join ', ')) }
else { Write-Output 'No image produced.' }
