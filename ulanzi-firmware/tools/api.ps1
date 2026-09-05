<#
  api.ps1 - eine Schnittstelle des Geraets abrufen und ansehen.
  Immer als Datei starten (-File): bei -Command frisst die Aufrufkette die
  Dollarzeichen.
#>
param(
  [string]$Ip   = '192.168.100.13',
  [string]$Pfad = '/api/config',
  [int]   $Zeig = 700
)
$ErrorActionPreference = 'Continue'
$url = "http://$Ip$Pfad"
try {
  $r = Invoke-WebRequest -Uri $url -TimeoutSec 20 -UseBasicParsing
} catch {
  Write-Output ("Fehler bei " + $url + ": " + $_.Exception.Message)
  exit 1
}
Write-Output ("HTTP {0}  {1}  {2} Bytes" -f [int]$r.StatusCode, $r.Headers['Content-Type'], $r.RawContentLength)
$txt = $r.Content
if ($txt -isnot [string]) { $txt = [System.Text.Encoding]::UTF8.GetString($r.Content) }
Write-Output ("Laenge: " + $txt.Length)
Write-Output $txt.Substring(0, [Math]::Min($Zeig, $txt.Length))
