# Listet alle Woerter in webui.cpp, die eine Umlaut-Umschrift enthalten.
$p = "$PSScriptRoot\..\src\webui.cpp"
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)

$words = [regex]::Matches($t, '[A-Za-z]*(ae|oe|ue|Ae|Oe|Ue)[A-Za-z]*') |
         ForEach-Object { $_.Value } | Sort-Object -Unique
Write-Output '=== AE/OE/UE ==='
$words | ForEach-Object { Write-Output $_ }

$ss = [regex]::Matches($t, '[A-Za-z]*ss[A-Za-z]*') |
      ForEach-Object { $_.Value } | Sort-Object -Unique
Write-Output '=== SS ==='
$ss | ForEach-Object { Write-Output $_ }
