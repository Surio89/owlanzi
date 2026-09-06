<#
  logo-embed.ps1 - turns assets\branding\owlanzi.svg into the PROGMEM literal
  LOGO inside src\webui.cpp.

  Why a script rather than doing it by hand: the artwork leaves the drawing
  program with 13 kB of indentation and 169 separate fill attributes. On the
  ESP32 every byte counts twice - it sits in flash AND goes over Wi-Fi on
  every page load. Here the fill moves onto the group and the whitespace
  goes; pixel for pixel the same picture is drawn. New version of the logo?
  Just run this again.

  ---------------------------------------------------------------------------
  PowerShell traps this project has walked into three times already:

   1. ALWAYS start it as a file with -File. With  powershell -Command "..."
      the call chain eats the dollar signs; what is left is gibberish plus an
      error message pointing somewhere else entirely.
   2. Variable names are NOT case sensitive. $a and $A are one and the same
      variable - that is how every umlaut once came out in capitals. Hence
      the deliberately distinct names below.
   3. In an array literal the comma binds tighter than the plus:
      ('a' + $x, 'b') is ONE element, not two.
  ---------------------------------------------------------------------------
#>
param(
  [string]$Logo = "$PSScriptRoot\..\assets\branding\owlanzi.svg",
  [string]$Cpp  = "$PSScriptRoot\..\src\webui.cpp"
)
$ErrorActionPreference = 'Stop'

function Show([string]$text) { Write-Output $text }

$svgRaw = [System.IO.File]::ReadAllText($Logo)
Show ("source : {0}  ({1} bytes)" -f $Logo, $svgRaw.Length)

# --- take the viewBox from the file, do not guess it ----------------------
$viewBox = [regex]::Match($svgRaw, '<svg[^>]*viewBox="([^"]*)"').Groups[1].Value
if (-not $viewBox) { throw 'No viewBox found in the logo.' }

# --- collect the tiles of the pixel owl -----------------------------------
$owlGroup = [regex]::Match($svgRaw, '<g id="pixel-owl">(.*?)</g>', 'Singleline').Groups[1].Value
if (-not $owlGroup) { throw 'Group pixel-owl not found.' }

$tileList = @()
foreach ($hit in [regex]::Matches($owlGroup, '<rect\s+([^/>]*)/>')) {
  $attr = @{}
  foreach ($pair in [regex]::Matches($hit.Groups[1].Value, '([\w-]+)="([^"]*)"')) {
    $attr[$pair.Groups[1].Value] = $pair.Groups[2].Value
  }
  $tileList += , $attr
}
if ($tileList.Count -lt 100) { throw "Only $($tileList.Count) tiles - the structure of the file has changed." }

# All tiles must be the same size, otherwise width/height/rx must not be
# copied from the first one.
$shapes = $tileList | ForEach-Object { "$($_.width)/$($_.height)/$($_.rx)" } | Sort-Object -Unique
if ($shapes.Count -ne 1) { throw ("Tiles differ in size: " + ($shapes -join ', ')) }

$tileW  = $tileList[0].width
$tileH  = $tileList[0].height
$tileRx = $tileList[0].rx

# Positions must be unique - only then is reordering by colour harmless
# (rectangles drawn later would otherwise end up on top).
$places = $tileList | ForEach-Object { "$($_.x),$($_.y)" } | Sort-Object -Unique
if ($places.Count -ne $tileList.Count) { throw 'Tiles overlap - reordering by colour would not be harmless.' }

$fillGroups = $tileList | Group-Object { $_.fill } | Sort-Object Count -Descending
$mainFill   = $fillGroups[0].Name
Show ("tiles  : {0}, size {1}x{2} rx {3}, colours: {4}" -f $tileList.Count, $tileW, $tileH, $tileRx,
      (($fillGroups | ForEach-Object { "$($_.Name) x$($_.Count)" }) -join ', '))

# A space before the "/>" is MANDATORY. Without quotes the HTML parser would
# otherwise swallow the slash as part of the attribute value - the element
# would not self-close and everything after it would end up nested inside.
$build = New-Object System.Text.StringBuilder
foreach ($tile in $tileList) {
  if ($tile.fill -ne $mainFill) { continue }
  [void]$build.Append("<rect x=$($tile.x) y=$($tile.y) width=$tileW height=$tileH rx=$tileRx />")
}
foreach ($tile in $tileList) {
  if ($tile.fill -eq $mainFill) { continue }
  [void]$build.Append("<rect x=$($tile.x) y=$($tile.y) width=$tileW height=$tileH rx=$tileRx fill=""$($tile.fill)"" />")
}

# --- wordmark unchanged, only without the line breaks ---------------------
$wordmark = [regex]::Match($svgRaw, '<g id="wordmark".*?</g>', 'Singleline').Value
if (-not $wordmark) { throw 'Group wordmark not found.' }
$wordmark = [regex]::Replace($wordmark, '\s*\r?\n\s*', '')

# fill=none belongs on the root: the wordmark is nothing but strokes. Without
# the attribute the browser would fill its paths black.
$svgOut = '<svg viewBox="' + $viewBox + '" xmlns="http://www.w3.org/2000/svg" fill=none' +
          ' class=logo role=img aria-label=owlanzi>' +
          '<g fill="' + $mainFill + '">' + $build.ToString() + '</g>' + $wordmark + '</svg>'

if ($svgOut -match '[\r\n]') { throw 'The generated SVG contains line breaks.' }
Show ("SVG    : {0} bytes (from {1}, so {2} %)" -f $svgOut.Length, $svgRaw.Length,
      [int](100 * $svgOut.Length / $svgRaw.Length))

# --- splice it into webui.cpp ---------------------------------------------
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$cppText   = [System.IO.File]::ReadAllText($Cpp, $utf8NoBom)

$literalRx = '(?m)^static const char (?:OWL|LOGO)\[\] PROGMEM = R"(?:OWL|LOGO)\(.*\)(?:OWL|LOGO)";'
$found     = [regex]::Matches($cppText, $literalRx)
if ($found.Count -ne 1) { throw "Expected exactly one logo literal, found: $($found.Count)." }
$oldLen = $found[0].Value.Length

$newLine   = 'static const char LOGO[] PROGMEM = R"LOGO(' + $svgOut + ')LOGO";'
$evaluator = [System.Text.RegularExpressions.MatchEvaluator] { param($m) $newLine }
$cppNew    = [regex]::Replace($cppText, $literalRx, $evaluator)

# The literal was found (otherwise we would have thrown above). Identical
# text therefore means: already in step with the artwork - not an error.
if ($cppNew -eq $cppText) {
  Show 'Literal already matches the artwork - nothing to do.'
  exit 0
}

[System.IO.File]::WriteAllText("$Cpp.bak", $cppText, $utf8NoBom)
[System.IO.File]::WriteAllText($Cpp,       $cppNew,  $utf8NoBom)

Show ("literal: {0} -> {1} bytes" -f $oldLen, $newLine.Length)
Show ("file   : {0} -> {1} bytes (backup as webui.cpp.bak)" -f $cppText.Length, $cppNew.Length)

# --- check: are the umlauts still intact? ---------------------------------
$census = @{}
foreach ($ch in $cppNew.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $key = 'U+{0:X4}' -f [int]$ch
    if ($census.ContainsKey($key)) { $census[$key]++ } else { $census[$key] = 1 }
  }
}
Show ("non-ASCII: " + (($census.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Key) x$($_.Value)" }) -join ', '))
