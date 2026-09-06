<#
  api.ps1 - fetch one of the device endpoints and look at it.

  Always run as a file (-File): with -Command the call chain eats the dollar
  signs and what arrives is gibberish.
#>
param(
  [string]$Ip   = '192.168.100.13',
  [string]$Path = '/api/config',
  [int]   $Show = 700
)
$ErrorActionPreference = 'Continue'
$url = "http://$Ip$Path"
try {
  $r = Invoke-WebRequest -Uri $url -TimeoutSec 20 -UseBasicParsing
} catch {
  Write-Output ("failed at " + $url + ": " + $_.Exception.Message)
  exit 1
}
Write-Output ("HTTP {0}  {1}  {2} bytes" -f [int]$r.StatusCode, $r.Headers['Content-Type'], $r.RawContentLength)
$txt = $r.Content
if ($txt -isnot [string]) { $txt = [System.Text.Encoding]::UTF8.GetString($r.Content) }
Write-Output ("length: " + $txt.Length)
Write-Output $txt.Substring(0, [Math]::Min($Show, $txt.Length))
