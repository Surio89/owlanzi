# Pulls the <script> blocks out of the served page and has node ONLY check
# them (--check), not run them. A syntax error would take the whole interface
# down, not just the one feature that introduced it.
$src = "$PSScriptRoot\..\served.html"
$html = [System.IO.File]::ReadAllText($src, [System.Text.Encoding]::UTF8)

$m = [regex]::Matches($html, '(?s)<script>(.*?)</script>')
Write-Output ("script blocks: " + $m.Count)

$all = ($m | ForEach-Object { $_.Groups[1].Value }) -join "`n"
$js  = "$PSScriptRoot\..\served.js"
[System.IO.File]::WriteAllText($js, $all, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("lines total: " + ($all -split "`n").Count)

$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { Write-Output 'node not found - syntax check skipped.'; exit }

# --check reports syntax errors but executes nothing.
& node --check $js 2>&1 | ForEach-Object { Write-Output ('  ' + $_) }
if ($LASTEXITCODE -eq 0) { Write-Output 'JavaScript: syntax ok.' }
else { Write-Output ('JavaScript: SYNTAX ERROR (exit ' + $LASTEXITCODE + ')') }
