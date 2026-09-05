# ---------------------------------------------------------------------------
#  owlanzi - echte Umlaute in der deutschen Weboberflaeche
#
#  Fasst AUSSCHLIESSLICH src/webui.cpp an. config.cpp, display.cpp und
#  main.cpp bleiben unberuehrt: deren deutsche Texte gehen auf die Matrix,
#  und der 3x5-Font kennt nur ASCII 32..90.
#
#  ZWEI PowerShell-Fallen, beide hier schon einmal zugeschnappt:
#
#  1. Hashtable-Schluessel sind case-insensitiv. 'Zurueck' und 'zurueck'
#     in einem Literal sind ein Duplikat und brechen den Parser. Deshalb
#     ein geordnetes Array aus Paaren statt eines Hashtables.
#
#  2. VARIABLENNAMEN sind ebenfalls case-insensitiv. $a und $A sind
#     dieselbe Variable - "$a=ae; $A=Ae" ueberschreibt still das erste,
#     und alle Umlaute kommen gross heraus. Deshalb unten $aeL/$aeU.
#
#  Die Wortliste ist kuratiert, nicht generisch: 'aktuell', 'Dauer',
#  'neue', 'Sauerstoff', 'zusammenzubauen', 'blue', 'true', 'value',
#  'continue', 'querySelector' enthalten dieselben Buchstabenpaare und
#  duerfen NICHT angefasst werden. Eine Regel ue->ue zerstoerte den Code.
#
#  Reihenfolge: lange Woerter zuerst, sonst zerschneidet 'ueber' das
#  'uebertragen' und 'fuer' das 'dafuer'.
#
#  Geschrieben wird UTF-8 OHNE BOM. Set-Content wuerde unter Windows
#  PowerShell 5.1 ANSI schreiben - aus ae wuerde ein einzelnes Byte 0xE4
#  (Latin-1), im Browser dann ein Fragezeichen.
# ---------------------------------------------------------------------------

param(
  [string]$Path = "$PSScriptRoot\..\src\webui.cpp",
  [switch]$Apply
)

$aeL=[char]0xE4; $oeL=[char]0xF6; $ueL=[char]0xFC   # ae oe ue
$aeU=[char]0xC4; $oeU=[char]0xD6; $ueU=[char]0xDC   # Ae Oe Ue
$szL=[char]0xDF                                     # ss

$pairs = @(
  ,@('Farbaenderungen', "Farb${aeL}nderungen")
  ,@('Zurueckgesetzt',  "Zur${ueL}ckgesetzt")
  ,@('gefaehrlichste',  "gef${aeL}hrlichste")
  ,@('vorzutaeuschen',  "vorzut${aeL}uschen")
  ,@('zurueckgesetzt',  "zur${ueL}ckgesetzt")
  ,@('zuruecksetzen',   "zur${ueL}cksetzen")
  ,@('Uebersetzungs',   "${ueU}bersetzungs")
  ,@('unveraendert',    "unver${aeL}ndert")
  ,@('vollstaendig',    "vollst${aeL}ndig")
  ,@('Farbwaehlern',    "Farbw${aeL}hlern")
  ,@('uebertragen',     "${ueL}bertragen")
  ,@('Lautstaerke',     "Lautst${aeL}rke")
  ,@('Oberflaeche',     "Oberfl${aeL}che")
  ,@('Schluessel',      "Schl${ueL}ssel")
  ,@('uebersetzt',      "${ueL}bersetzt")
  ,@('auswaehlen',      "ausw${aeL}hlen")
  ,@('Aenderung',       "${aeU}nderung")
  ,@('ergaenzen',       "erg${aeL}nzen")
  ,@('ergaenzt',        "erg${aeL}nzt")
  ,@('Zaehlung',        "Z${aeL}hlung")
  ,@('Waehrend',        "W${aeL}hrend")
  ,@('Fuellung',        "F${ueL}llung")
  ,@('loeschen',        "l${oeL}schen")
  ,@('moeglich',        "m${oeL}glich")
  ,@('muessen',         "m${ueL}ssen")
  ,@('gueltig',         "g${ueL}ltig")
  ,@('hoerbar',         "h${oeL}rbar")
  ,@('oeffnen',         "${oeL}ffnen")
  ,@('oeffnet',         "${oeL}ffnet")
  ,@('Loescht',         "L${oeL}scht")
  ,@('stoeren',         "st${oeL}ren")
  ,@('pruefen',         "pr${ueL}fen")
  ,@('Zurueck',         "Zur${ueL}ck")
  ,@('dafuer',          "daf${ueL}r")
  ,@('Geraet',          "Ger${aeL}t")
  ,@('Haengt',          "H${aeL}ngt")
  ,@('laeuft',          "l${aeL}uft")
  ,@('laesst',          "l${aeL}sst")
  ,@('prueft',          "pr${ueL}ft")
  ,@('Stueck',          "St${ueL}ck")
  ,@('faellt',          "f${aeL}llt")
  ,@('heissen',         "hei${szL}en")
  ,@('gruen',           "gr${ueL}n")
  ,@('ueber',           "${ueL}ber")
  ,@('waere',           "w${aeL}re")
  ,@('laedt',           "l${aeL}dt")
  ,@('loest',           "l${oeL}st")
  ,@('weiss',           "wei${szL}")
  ,@('Fuer',            "F${ueL}r")
  ,@('fuer',            "f${ueL}r")
)

if (-not (Test-Path -LiteralPath $Path)) { throw "Nicht gefunden: $Path" }

$text = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
$orig = $text
$total = 0

foreach ($p in $pairs) {
  $from = $p[0]; $to = $p[1]
  $n = ([regex]::Matches($text, [regex]::Escape($from))).Count
  if ($n -gt 0) {
    $text = $text.Replace($from, $to)     # ordinal, also case-sensitiv
    $total += $n
  }
}
Write-Output ("Summe: {0} Ersetzungen" -f $total)

# Gegenprobe: welche Codepunkte sind entstanden? Muessen die KLEINEN sein.
$counts = @{}
foreach ($ch in $text.ToCharArray()) {
  if ([int]$ch -gt 127) {
    $k = 'U+{0:X4}' -f [int]$ch
    if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
  }
}
Write-Output 'Codepunkte:'
foreach ($k in ($counts.Keys | Sort-Object)) { Write-Output ('  {0} x{1}' -f $k, $counts[$k]) }

Write-Output 'Nicht angefasst (muss alles englisch/Code sein):'
$rest = [regex]::Matches($text, '[A-Za-z]*(ae|oe|ue|Ae|Oe|Ue)[A-Za-z]*') |
        ForEach-Object { $_.Value } | Sort-Object -Unique -CaseSensitive
$rest | ForEach-Object { Write-Output ('  ' + $_) }

if ($Apply) {
  if ($text -eq $orig) { Write-Output 'Keine Aenderung.'; exit }
  [System.IO.File]::WriteAllText("$Path.bak", $orig, (New-Object System.Text.UTF8Encoding($false)))
  [System.IO.File]::WriteAllText($Path,       $text, (New-Object System.Text.UTF8Encoding($false)))
  Write-Output "Geschrieben. Sicherung: $Path.bak"
} else {
  Write-Output 'PROBELAUF - nichts geschrieben. Mit -Apply erneut aufrufen.'
}
