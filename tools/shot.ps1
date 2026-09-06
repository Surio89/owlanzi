<#
  shot.ps1 - screenshot of a page without a visible window.

  Lets you check what the served interface actually looks like instead of
  only reading its source. Uses Chrome, falls back to Edge.

  Call it as a file (see trap 1 in logo-embed.ps1):
    powershell -File tools\shot.ps1 -Url "http://192.168.100.13/" -Width 900
#>
param(
  [string]$Url    = 'http://192.168.100.13/',
  [int]   $Width  = 900,
  [int]   $Height = 700,
  [string]$Out    = "$PSScriptRoot\..\shot.png",
  [int]   $Scale  = 2,
  # The new headless mode forces a window width of at least 500 px - anything
  # narrower is silently raised to 500 and the image is merely cropped
  # afterwards. Use 'old' when checking phone widths.
  [ValidateSet('new', 'old')]
  [string]$Headless = 'new'
)
$ErrorActionPreference = 'Stop'

$candidates = @(
  'C:\Program Files\Google\Chrome\Application\chrome.exe',
  'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe',
  'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
  'C:\Program Files\Microsoft\Edge\Application\msedge.exe'
)
$browser = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $browser) { throw 'Neither Chrome nor Edge found.' }

if (Test-Path $Out) { Remove-Item $Out }

# Not $profile and not $args: both are automatic PowerShell variables, and
# quietly shadowing them is the kind of trap this project has been caught by
# before.
$profileDir = Join-Path $env:TEMP ('owlanzi-shot-' + [guid]::NewGuid().ToString('N'))
$argList = @(
  "--headless=$Headless"
  '--disable-gpu'
  '--hide-scrollbars'
  "--user-data-dir=$profileDir"
  "--window-size=$Width,$Height"
  "--force-device-scale-factor=$Scale"
  # Generous: the page fetches its settings first and only then draws the
  # previews. Too tight a budget and you get an image full of black boxes.
  '--virtual-time-budget=9000'
  "--screenshot=$Out"
  $Url
)
# Chrome reports success on the error channel ("... bytes written to file").
# With ErrorActionPreference=Stop PowerShell would treat that as a failure,
# so carry on explicitly here and throw the channel away.
$ErrorActionPreference = 'Continue'
& $browser @argList 2>$null | Out-Null
$ErrorActionPreference = 'Stop'

if (Test-Path $profileDir) { Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue }

if (Test-Path $Out) {
  $size = (Get-Item $Out).Length
  Write-Output ("{0} ({1} bytes) via {2}" -f $Out, $size, (Split-Path $browser -Leaf))
} else {
  throw 'No image produced.'
}
