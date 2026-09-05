# Zieht die <script>-Bloecke aus der ausgelieferten Seite und laesst node
# sie NUR pruefen (--check), nicht ausfuehren. Ein Syntaxfehler wuerde die
# ganze Oberflaeche lahmlegen, nicht bloss die Platzhalter.
$src = "$PSScriptRoot\..\served.html"
$html = [System.IO.File]::ReadAllText($src, [System.Text.Encoding]::UTF8)

$m = [regex]::Matches($html, '(?s)<script>(.*?)</script>')
Write-Output ("Script-Bloecke: " + $m.Count)

$all = ($m | ForEach-Object { $_.Groups[1].Value }) -join "`n"
$js  = "$PSScriptRoot\..\served.js"
[System.IO.File]::WriteAllText($js, $all, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Zeilen gesamt: " + ($all -split "`n").Count)

$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { Write-Output 'node nicht gefunden - Syntaxpruefung uebersprungen.'; exit }

# --check meldet Syntaxfehler, fuehrt aber nichts aus.
& node --check $js 2>&1 | ForEach-Object { Write-Output ('  ' + $_) }
if ($LASTEXITCODE -eq 0) { Write-Output 'JavaScript: Syntax in Ordnung.' }
else { Write-Output ('JavaScript: SYNTAXFEHLER (exit ' + $LASTEXITCODE + ')') }
