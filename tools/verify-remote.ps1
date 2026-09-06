<#
  verify-remote.ps1 - looks at what is actually sitting on GitHub.

  Not what was meant to be pushed, but what the server hands out. For a
  public repository that is the only view that counts.

  The response is parsed with ConvertFrom-Json rather than "gh --jq": the
  built-in filter silently returned nothing in this environment, and a
  checking tool that says "nothing found" both on success and on failure is
  worse than no tool at all.

  Always run as a file (-File).
#>
param([string]$Repo = 'Surio89/owlanzi', [string]$Branch = 'main')
$ErrorActionPreference = 'Continue'
$gh = 'C:\Program Files\GitHub CLI\gh.exe'
if (-not (Test-Path $gh)) { $gh = 'gh' }

$raw = & $gh api ("repos/{0}/git/trees/{1}?recursive=1" -f $Repo, $Branch) | Out-String
if (-not $raw.Trim()) { throw 'No answer from GitHub - signed in? repository there?' }
$tree = $raw | ConvertFrom-Json
if ($tree.truncated) { Write-Output 'WARNING: GitHub truncated the listing, it is incomplete.' }

$paths = $tree.tree | Where-Object { $_.type -eq 'blob' } | ForEach-Object { $_.path }
Write-Output ("files on branch {0}: {1}" -f $Branch, $paths.Count)

Write-Output ''
Write-Output 'Check - none of this may be public:'
$forbidden = @('secrets_local\.h$', 'tools/scratch/', '\.pio/', 'backup/', '\.bak$', 'served\.')
foreach ($f in $forbidden) {
  $hit = $paths | Where-Object { $_ -match $f }
  if ($hit) { Write-Output ('  FOUND  ' + $f); $hit | ForEach-Object { Write-Output ('     ' + $_) } }
  else      { Write-Output ('  clean  ' + $f) }
}

Write-Output ''
Write-Output 'Top level:'
$paths | Where-Object { $_ -notmatch '/' } | ForEach-Object { Write-Output ('  ' + $_) }
$paths | Where-Object { $_ -match '/' } | ForEach-Object { ($_ -split '/')[0] } |
  Sort-Object -Unique | ForEach-Object { Write-Output ('  ' + $_ + '/') }

Write-Output ''
$info = (& $gh api ("repos/{0}" -f $Repo) | Out-String | ConvertFrom-Json)
Write-Output ("visibility: {0}   size: {1} kB" -f $info.visibility, $info.size)
Write-Output ("URL: " + $info.html_url)
