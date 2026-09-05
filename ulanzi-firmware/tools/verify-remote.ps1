<#
  verify-remote.ps1 - sieht nach, was auf GitHub tatsaechlich liegt.

  Nicht was gepusht werden sollte, sondern was der Server ausliefert. Bei
  einem oeffentlichen Repo ist das der einzige Blick, der zaehlt.

  Die Antwort wird hier mit ConvertFrom-Json ausgewertet und nicht mit
  "gh --jq": der eingebaute Filter lieferte in dieser Umgebung stumm nichts
  zurueck, und ein Pruefwerkzeug, das bei Erfolg wie bei Fehler "nichts
  gefunden" sagt, ist schlimmer als keines.

  Immer als Datei starten (-File).
#>
param([string]$Repo = 'Surio89/owlanzi', [string]$Zweig = 'main')
$ErrorActionPreference = 'Continue'
$gh = 'C:\Program Files\GitHub CLI\gh.exe'
if (-not (Test-Path $gh)) { $gh = 'gh' }

$roh = & $gh api ("repos/{0}/git/trees/{1}?recursive=1" -f $Repo, $Zweig) | Out-String
if (-not $roh.Trim()) { throw 'Keine Antwort von GitHub - angemeldet? Repo vorhanden?' }
$baum = $roh | ConvertFrom-Json
if ($baum.truncated) { Write-Output 'ACHTUNG: GitHub hat die Liste gekuerzt, sie ist unvollstaendig.' }

$pfade = $baum.tree | Where-Object { $_.type -eq 'blob' } | ForEach-Object { $_.path }
Write-Output ("Dateien im Zweig {0}: {1}" -f $Zweig, $pfade.Count)

Write-Output ''
Write-Output 'Kontrolle - nichts davon darf oeffentlich sein:'
$verboten = @('secrets_local\.h$', 'tools/scratch/', '\.pio/', 'backup/', 'owlanzi-website/', '\.bak$')
foreach ($v in $verboten) {
  $t = $pfade | Where-Object { $_ -match $v }
  if ($t) { Write-Output ('  GEFUNDEN  ' + $v); $t | ForEach-Object { Write-Output ('     ' + $_) } }
  else    { Write-Output ('  sauber    ' + $v) }
}

Write-Output ''
Write-Output 'Oberste Ebene:'
$pfade | Where-Object { $_ -notmatch '/' } | ForEach-Object { Write-Output ('  ' + $_) }
$pfade | Where-Object { $_ -match '/' } | ForEach-Object { ($_ -split '/')[0] } |
  Sort-Object -Unique | ForEach-Object { Write-Output ('  ' + $_ + '/') }

Write-Output ''
$info = (& $gh api ("repos/{0}" -f $Repo) | Out-String | ConvertFrom-Json)
Write-Output ("Sichtbarkeit: {0}   Groesse: {1} kB" -f $info.visibility, $info.size)
Write-Output ("URL: " + $info.html_url)
