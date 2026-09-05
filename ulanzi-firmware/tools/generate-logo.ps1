$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$outputDir = Join-Path $PSScriptRoot '../assets/branding'
[void][System.IO.Directory]::CreateDirectory($outputDir)

# Reference owl transcribed to a regular vector grid. Lighter eyes suit dark backgrounds.
$rows = @(
    '.CC...........CC.',
    '.CCC.........CCC.',
    '..CCCCCCCCCCCCC..',
    '..CCCCCCCCCCCCC..',
    '.CC....CCC....CC.',
    '.C......C......C.',
    'CC..EE..C..EE..CC',
    'CC.E..E...E..E.CC',
    'CC......C......CC',
    'CC.....CCC.....CC',
    '.CC.....C.....CC.',
    'C..CCCC...CCCC..C',
    'CC..CCCCCCCCC..CC',
    'CCCC.CCCCCCC.CCCC',
    '.CCCC.CCCCC.CCCC.',
    '.CCCC.CCCCC.CCCC.',
    '..CCC.CCCCC.CCC..',
    '...CC.CCCCC.CC...',
    '......CCCCC......'
)
$colors = @{ C = '#59E6CF'; E = '#B7B5C9' }
$pixels = [System.Collections.Generic.List[object]]::new()
for ($row = 0; $row -lt $rows.Count; $row++) {
    if ($rows[$row].Length -ne 17) { throw "Invalid grid row $row" }
    for ($col = 0; $col -lt 17; $col++) {
        $key = [string]$rows[$row][$col]
        if ($colors.ContainsKey($key)) { $pixels.Add(@{ X = $col * 11; Y = $row * 11; Color = $colors[$key] }) }
    }
}
# Custom lettering inspired by the rounded reference, independent of installed fonts.
$strokes = @(
    @{ Id = 'w'; D = 'M150 99.5L150 160.5C150 178.173 164.327 192.5 182 192.5C199.673 192.5 214 178.173 214 160.5L214 112L214 160.5C214 178.173 228.327 192.5 246 192.5C263.673 192.5 278 178.173 278 160.5L278 99.5' },
    @{ Id = 'l'; D = 'M322 59.5L322 192.5' },
    @{ Id = 'a-stem'; D = 'M460.5 146L460.5 192.5' },
    @{ Id = 'n'; D = 'M505 192.5L505 141.25C505 118.192 523.692 99.5 546.75 99.5C569.808 99.5 588.5 118.192 588.5 141.25L588.5 192.5' },
    @{ Id = 'z'; D = 'M633 99.5L710 99.5L633 192.5L710 192.5' },
    @{ Id = 'i'; D = 'M757 103L757 192.5' }
)
$svg = [System.Collections.Generic.List[string]]::new()
$svg.Add('<svg xmlns="http://www.w3.org/2000/svg" width="1015.5" height="207" viewBox="0 0 1015.5 207" fill="none" role="img" aria-labelledby="owlanzi-title">')
$svg.Add('  <title id="owlanzi-title">owlanzi</title>')
$svg.Add('  <desc>Turquoise pixel owl with smiling eyes, a rounded off-white wordmark and a pink i dot. Transparent background, tightly cropped.</desc>')
$svg.Add('  <g id="pixel-owl">')
foreach ($pixel in $pixels) { $svg.Add("    <rect x=`"$($pixel.X)`" y=`"$($pixel.Y)`" width=`"9`" height=`"9`" rx=`"0.8`" fill=`"$($pixel.Color)`"/>") }
$svg.Add('  </g>')
$svg.Add('  <g id="wordmark" transform="translate(244 0)" stroke="#F5F4FA" stroke-width="29" stroke-linecap="round" stroke-linejoin="round">')
$svg.Add('    <circle id="o" cx="61" cy="146" r="46.5"/>')
$svg.Add('    <circle id="a-bowl" cx="414" cy="146" r="46.5"/>')
foreach ($stroke in $strokes) { $svg.Add("    <path id=`"$($stroke.Id)`" d=`"$($stroke.D)`"/>") }
$svg.Add('    <rect id="i-dot" x="742.5" y="50" width="29" height="27" rx="9" fill="#F277B7" stroke="none"/>')
$svg.Add('  </g>')
$svg.Add('</svg>')
[System.IO.File]::WriteAllText((Join-Path $outputDir 'owlanzi.svg'), ($svg -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))

function New-RoundedBox([single]$x, [single]$y, [single]$w, [single]$h, [single]$radius) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $d = $radius * 2
    $path.AddArc($x,$y,$d,$d,180,90)
    $path.AddArc($x+$w-$d,$y,$d,$d,270,90)
    $path.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90)
    $path.AddArc($x,$y+$h-$d,$d,$d,90,90)
    $path.CloseFigure()
    return ,$path
}
function New-StrokePath([string]$data) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $tokens = [regex]::Matches($data, '[MLC]|-?\d+(?:\.\d+)?') | ForEach-Object { $_.Value }
    $x = [single]0; $y = [single]0; $i = 0
    while ($i -lt $tokens.Count) {
        $command = $tokens[$i++]
        if ($command -eq 'M') { $x = [single]$tokens[$i++]; $y = [single]$tokens[$i++]; $path.StartFigure() }
        elseif ($command -eq 'L') {
            $nx = [single]$tokens[$i++]; $ny = [single]$tokens[$i++]
            $path.AddLine($x,$y,$nx,$ny); $x=$nx; $y=$ny
        } elseif ($command -eq 'C') {
            $x1=[single]$tokens[$i++]; $y1=[single]$tokens[$i++]; $x2=[single]$tokens[$i++]; $y2=[single]$tokens[$i++]; $nx=[single]$tokens[$i++]; $ny=[single]$tokens[$i++]
            $path.AddBezier($x,$y,$x1,$y1,$x2,$y2,$nx,$ny); $x=$nx; $y=$ny
        } else { throw "Unsupported path command $command" }
    }
    return ,$path
}
# Render identical geometry on a dark surface for the review image.
$bitmap = [System.Drawing.Bitmap]::new(1644,431)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#101018'))
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.ScaleTransform(1.5,1.5)
$graphics.TranslateTransform(40,40)
foreach ($pixel in $pixels) {
    $shape = New-RoundedBox $pixel.X $pixel.Y 9 9 0.8
    $brush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml($pixel.Color))
    $graphics.FillPath($brush,$shape)
    $shape.Dispose(); $brush.Dispose()
}
$graphics.TranslateTransform(244,0)
$pen = [System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml('#F5F4FA'),29)
$pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
$graphics.DrawEllipse($pen,14.5,99.5,93,93)
$graphics.DrawEllipse($pen,367.5,99.5,93,93)
foreach ($stroke in $strokes) {
    $path = New-StrokePath $stroke.D
    $graphics.DrawPath($pen,$path)
    $path.Dispose()
}
$dot = New-RoundedBox 742.5 50 29 27 9
$pink = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#F277B7'))
$graphics.FillPath($pink,$dot)
$bitmap.Save((Join-Path $outputDir 'owlanzi-preview.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$dot.Dispose(); $pink.Dispose(); $pen.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
Write-Output 'Created assets/branding/owlanzi.svg (1015.5 x 207) and dark preview.'
