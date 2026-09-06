// Checks the served page against its own translation table:
//  - does every data-i18n key have a German entry?
//  - does every data-i18n-ph key have a German entry?
//  - are there German entries no element uses any more?
// Runs without a browser and without jsdom - plain text analysis.

const fs = require('fs');
const path = require('path');
const file = process.argv[2] || path.join(__dirname, '..', 'served.html');
console.log('file: ' + file);
const html = fs.readFileSync(file, 'utf8');

// --- cut out the DE table -------------------------------------------------
// Count braces, so that curly braces inside the text do not confuse us.
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
if (!deSrc) { console.log('DE table not found'); process.exit(1); }
const DE = eval('(' + deSrc + ')');
console.log('DE entries: ' + Object.keys(DE).length);

// --- keys from the markup -------------------------------------------------
const used = new Set();
for (const m of html.matchAll(/data-i18n=([A-Za-z0-9_]+)/g))     used.add(m[1]);
for (const m of html.matchAll(/data-i18n="([^"]+)"/g))           used.add(m[1]);
const usedPh = new Set();
for (const m of html.matchAll(/data-i18n-ph=([A-Za-z0-9_]+)/g))  usedPh.add(m[1]);
for (const m of html.matchAll(/data-i18n-ph="([^"]+)"/g))        usedPh.add(m[1]);

// data-i18n-ph=x does not also match the data-i18n regex above - separate
// patterns, which is what we want.
console.log('data-i18n in the markup: ' + used.size);
console.log('data-i18n-ph in the markup: ' + usedPh.size + '  -> ' + [...usedPh].join(', '));

const missing   = [...used].filter(k => DE[k] === undefined);
const missingPh = [...usedPh].filter(k => DE[k] === undefined);
const unused    = Object.keys(DE).filter(k => !used.has(k) && !usedPh.has(k));

console.log('');
console.log('without a German translation: ' + (missing.length || 'none'));
missing.forEach(k => console.log('  ' + k));
console.log('placeholders without a translation: ' + (missingPh.length || 'none'));
missingPh.forEach(k => console.log('  ' + k));
console.log('DE entries with no element (may be used from code): ' + (unused.length || 'none'));
unused.forEach(k => console.log('  ' + k));

// --- the placeholders in detail -------------------------------------------
console.log('');
for (const m of html.matchAll(/<input[^>]*data-i18n-ph=([A-Za-z0-9_]+)[^>]*>/g)) {
  const tag = m[0];
  const id  = (tag.match(/id=([A-Za-z0-9_]+)/) || [])[1];
  const ph  = (tag.match(/placeholder="([^"]*)"/) || [])[1];
  console.log('  #' + id + '  EN: "' + ph + '"  DE: "' + DE[m[1]] + '"');
}
