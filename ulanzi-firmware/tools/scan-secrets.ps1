<#
  scan-secrets.ps1 - sucht Zugangsdaten im Projekt, bevor etwas oeffentlich wird.

  Die Suchbegriffe stehen NICHT in diesem Skript, sondern werden aus
  src\secrets_local.h gelesen. Diese Datei steht in .gitignore; ein Skript
  mit eingebauten Passwoertern waere selbst das Leck, das es verhindern soll.

  Durchsucht wird alles ausser .pio, .git, .vscode und backup.
  Immer als Datei starten (-File).
#>
param(
  [string]$Root    = "$PSScriptRoot\..",
  [string]$Secrets = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Secrets) { $Secrets = Join-Path $Root 'src\secrets_local.h' }

if (-not (Test-Path $Secrets)) { Write-Output 'Keine secrets_local.h gefunden - nichts zu vergleichen.'; exit 0 }

# Werte aus den #define-Zeilen ziehen.
$nadeln = @()
foreach ($z in (Get-Content $Secrets)) {
  $m = [regex]::Match($z, '#define\s+\w+\s+"([^"]+)"')
  if ($m.Success -and $m.Groups[1].Value.Length -ge 4) { $nadeln += $m.Groups[1].Value }
}
$nadeln = $nadeln | Sort-Object -Unique -CaseSensitive
Write-Output ("Suchbegriffe aus secrets_local.h: {0} Stueck (werden hier nicht ausgegeben)" -f $nadeln.Count)

$ausnahme = @('\.pio\', '\.git\', '\.vscode\', '\backup\', 'secrets_local.h')
$dateien = Get-ChildItem -Path $Root -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $p = $_.FullName; -not ($ausnahme | Where-Object { $p -like ('*' + $_ + '*') }) }

Write-Output ("Durchsuchte Dateien: {0}" -f $dateien.Count)

$treffer = @()
foreach ($f in $dateien) {
  $inhalt = $null
  try { $inhalt = [System.IO.File]::ReadAllText($f.FullName) } catch { continue }
  if (-not $inhalt) { continue }
  foreach ($n in $nadeln) {
    if ($inhalt.Contains($n)) {
      $treffer += [pscustomobject]@{ Datei = $f.FullName.Substring($Root.Length + 1); Laenge = $n.Length }
    }
  }
}

if ($treffer.Count) {
  Write-Output ''
  Write-Output 'GEFUNDEN - diese Dateien enthalten Zugangsdaten im Klartext:'
  $treffer | Sort-Object Datei | ForEach-Object { Write-Output ('  ' + $_.Datei + '   (Begriff mit ' + $_.Laenge + ' Zeichen)') }
} else {
  Write-Output ''
  Write-Output 'Sauber: kein Suchbegriff ausserhalb von secrets_local.h gefunden.'
}

# Die Bauergebnisse liegen in .pio und sind oben ausgenommen - gerade sie
# sind aber der gefaehrliche Fall: die Zeichenketten aus secrets_local.h
# landen beim Uebersetzen im Abbild. Hier der direkte Vergleich.
Write-Output ''
Write-Output 'Bauergebnisse (hier zeigt sich, ob -DNO_LOCAL_SECRETS greift):'
foreach ($b in (Get-ChildItem -Path (Join-Path $Root '.pio\build') -Recurse -Filter 'firmware.bin' -ErrorAction SilentlyContinue)) {
  $roh = [System.IO.File]::ReadAllBytes($b.FullName)
  $utf8 = New-Object System.Text.UTF8Encoding($false)
  $wieviele = 0
  foreach ($n in $nadeln) {
    if ($n.Length -lt 8) { continue }   # "owlet" ist auch der Markenname
    $nb = $utf8.GetBytes($n)
    for ($i = 0; $i -le $roh.Length - $nb.Length; $i++) {
      $ok = $true
      for ($j = 0; $j -lt $nb.Length; $j++) { if ($roh[$i + $j] -ne $nb[$j]) { $ok = $false; break } }
      if ($ok) { $wieviele++; break }
    }
  }
  $wo = $b.FullName.Substring($Root.Length + 1)
  if ($wieviele) { Write-Output ("  ACHTUNG  {0}: {1} Zugangsdaten im Abbild" -f $wo, $wieviele) }
  else           { Write-Output ("  sauber   {0}" -f $wo) }
}

# Bilder koennen Zugangsdaten zeigen, ohne sie als Text zu enthalten.
Write-Output ''
Write-Output 'Bilder im Projekt (bitte einzeln beurteilen - ein Bildschirmfoto kann eine E-Mail-Adresse zeigen):'
$dateien | Where-Object { $_.Extension -match '^\.(png|jpg|jpeg|gif|webp)$' } |
  ForEach-Object { Write-Output ('  ' + $_.FullName.Substring($Root.Length + 1)) }
