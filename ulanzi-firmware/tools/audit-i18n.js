// Prueft die ausgelieferte Seite gegen ihre eigene Uebersetzungstabelle:
//  - hat jeder data-i18n-Schluessel einen deutschen Eintrag?
//  - hat jeder data-i18n-ph-Schluessel einen deutschen Eintrag?
//  - gibt es deutsche Eintraege, die kein Element mehr benutzt?
// Laeuft ohne Browser, ohne jsdom - reine Textanalyse.

const fs = require('fs');
const file = process.argv[2] ||
  'C:/Users/Jannik/IdeaProjects/privat/owlanzi/served.html';
console.log('Datei: ' + file);
const html = fs.readFileSync(file, 'utf8');

// --- die DE-Tabelle herausschneiden ---------------------------------------
// Klammern zaehlen, damit geschweifte Klammern im Text nicht stoeren.
function grabObject(src, marker) {
  const start = src.indexOf(marker);
  if (start < 0) return null;
  let i = src.indexOf('{', start), depth = 0, inStr = null;
  const from = i;
  for (; i < src.length; i++) {
    const c = src[i];
    if (inStr) { if (c === '\\') i++; else if (c === inStr) inStr = null; continue; }
    if (c === "'" || c === '"') { inStr = c; continue; }
    if (c === '{') depth++;
    else if (c === '}') { depth--; if (depth === 0) return src.slice(from, i + 1); }
  }
  return null;
}

const deSrc = grabObject(html, 'const DE=');
if (!deSrc) { console.log('DE-Tabelle nicht gefunden'); process.exit(1); }
const DE = eval('(' + deSrc + ')');
console.log('DE-Eintraege: ' + Object.keys(DE).length);

// --- Schluessel aus dem Markup -------------------------------------------
const used = new Set();
for (const m of html.matchAll(/data-i18n=([A-Za-z0-9_]+)/g))     used.add(m[1]);
for (const m of html.matchAll(/data-i18n="([^"]+)"/g))           used.add(m[1]);
const usedPh = new Set();
for (const m of html.matchAll(/data-i18n-ph=([A-Za-z0-9_]+)/g))  usedPh.add(m[1]);
for (const m of html.matchAll(/data-i18n-ph="([^"]+)"/g))        usedPh.add(m[1]);

// data-i18n-ph=x matcht oben auch als data-i18n=... nicht - eigene Regex, gut.
console.log('data-i18n im Markup: ' + used.size);
console.log('data-i18n-ph im Markup: ' + usedPh.size + '  -> ' + [...usedPh].join(', '));

const fehlend = [...used].filter(k => DE[k] === undefined);
const fehlendPh = [...usedPh].filter(k => DE[k] === undefined);
const unbenutzt = Object.keys(DE).filter(k => !used.has(k) && !usedPh.has(k));

console.log('');
console.log('Ohne deutsche Uebersetzung: ' + (fehlend.length || 'keine'));
fehlend.forEach(k => console.log('  ' + k));
console.log('Platzhalter ohne Uebersetzung: ' + (fehlendPh.length || 'keine'));
fehlendPh.forEach(k => console.log('  ' + k));
console.log('DE-Eintraege ohne Element (evtl. per Code benutzt): ' + (unbenutzt.length || 'keine'));
unbenutzt.forEach(k => console.log('  ' + k));

// --- Platzhalter konkret --------------------------------------------------
console.log('');
for (const m of html.matchAll(/<input[^>]*data-i18n-ph=([A-Za-z0-9_]+)[^>]*>/g)) {
  const tag = m[0];
  const id  = (tag.match(/id=([A-Za-z0-9_]+)/) || [])[1];
  const ph  = (tag.match(/placeholder="([^"]*)"/) || [])[1];
  console.log('  #' + id + '  EN: "' + ph + '"  DE: "' + DE[m[1]] + '"');
}
