/*
 * webui.cpp - the interface and the JSON API
 *
 * The pages sit in flash as one piece and are served straight from there
 * with sendContent_P, without assembling HTML on the heap.
 *
 * LANGUAGE: the interface is written in English - that way anyone rebuilding
 * the device can read it. German comes from a translation table in the
 * browser; elements carry data-i18n keys for it. The firmware itself only
 * translates what ends up on the matrix (L()).
 *
 * SETUP: while the device sits in hotspot mode, handleRoot serves its own
 * slim page - Wi-Fi and Owlet account, nothing else. Somebody who has just
 * unboxed it should not have to hunt past colour pickers and alarm limits.
 */
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "webui.h"
#include "display.h"
#include "owlet.h"

static WebServer srv(80);

// --- shared look ------------------------------------------------------------
static const char CSS[] PROGMEM = R"CSS(<style>
:root{color-scheme:dark;--bg:#0c0e12;--card:#151920;--line:#232a35;--fg:#e8ebf0;--mut:#8d97a6;
 --acc:#3b82f6;--ok:#4ade80;--warn:#fbbf24;--bad:#f87171}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.55 system-ui,-apple-system,sans-serif}
.w{max-width:660px;margin:0 auto;padding:0 16px 40px}
.title{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.title h1{margin:0;line-height:0;flex:none}
svg.logo{height:40px;width:auto;flex:none;display:block}
@media(max-width:560px){svg.logo{height:32px}}
@media(max-width:400px){svg.logo{height:28px}}
.lang{margin-left:auto;display:flex;gap:3px;background:#12161d;padding:3px;border-radius:8px}
.lang button{background:none;border:0;color:var(--mut);padding:4px 9px;border-radius:6px;
 font-size:12px;font-weight:600;cursor:pointer}
.lang button.on{background:#232b38;color:var(--fg)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:15px;margin:12px 0}
.card>h2{font-size:12px;margin:0 0 11px;color:var(--mut);text-transform:uppercase;letter-spacing:.07em}
.row{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid #1b212b}
.row:last-child{border:0} .row>span:first-child{color:var(--mut)}
label{display:block;margin:12px 0 4px;color:#b3bcc9;font-size:13px}
input[type=text],input[type=password],input[type=number],select,input[type=file]{width:100%;
 padding:9px 10px;background:#0a0d12;color:var(--fg);border:1px solid #2b3340;border-radius:8px;font-size:15px}
input[type=range]{width:100%;accent-color:var(--acc)}
/* Password field with a show/hide eye. The wrapper is added by script, so the
   markup stays a plain input and nothing breaks if the script fails. */
.pw{position:relative}
.pw>input{padding-right:42px}
.pw>button{position:absolute;right:2px;top:2px;bottom:2px;width:36px;padding:0;
 display:flex;align-items:center;justify-content:center;background:none;border:0;
 color:var(--mut);cursor:pointer;border-radius:7px}
.pw>button:hover{color:var(--fg)}
.pw>button:focus-visible{outline:2px solid var(--acc);outline-offset:-2px}
.pw>button svg{width:19px;height:19px;fill:none;stroke:currentColor;stroke-width:1.6;
 stroke-linecap:round;stroke-linejoin:round}
.sl{display:flex;align-items:center;gap:11px}
.sl output{min-width:44px;text-align:right;font-variant-numeric:tabular-nums;color:var(--mut);font-size:13px}
.sw{display:flex;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid #1b212b}
.sw:last-child{border:0}
.sw input[type=color]{width:38px;height:26px;padding:0;border:1px solid #2b3340;border-radius:6px;
 background:none;cursor:pointer;flex:none}
.sw span{flex:1;font-size:14px}
.rs{flex:none;width:26px;height:26px;padding:0;background:none;border:1px solid #2b3340;
 border-radius:6px;color:var(--mut);font-size:14px;line-height:1;cursor:pointer}
.rs:hover{color:var(--fg);border-color:#3a4556}
.chk{display:flex;align-items:center;gap:9px;margin:11px 0}
.chk input{width:17px;height:17px;accent-color:var(--acc)}
button.b{padding:9px 14px;background:var(--acc);color:#fff;border:0;border-radius:8px;
 font-size:14px;font-weight:550;cursor:pointer;margin:4px 6px 0 0}
button.b.g{background:#28303d} button.b.r{background:#8f2f2f}
button.b.big{padding:13px 20px;font-size:16px;width:100%;margin-top:14px}
.note{color:var(--mut);font-size:12.5px;margin:9px 0 0}
.warnbox{background:#2a1e10;border:1px solid #4a3616;border-radius:9px;padding:11px;
 font-size:13px;color:#e8c98a;margin:10px 0}
.step{display:flex;align-items:center;gap:9px;margin:0 0 4px}
.step b{width:22px;height:22px;border-radius:50%;background:var(--acc);color:#fff;
 font-size:12px;display:flex;align-items:center;justify-content:center;flex:none}
.step h2{margin:0;font-size:15px;color:var(--fg);text-transform:none;letter-spacing:0}
canvas.pv{width:100%;display:block;background:#05060a;border:1px solid var(--line);
 border-radius:9px;margin:0 0 8px}
.pvcap{color:var(--mut);font-size:12px;margin:0 0 12px}
h3.sub{margin:16px 0 2px;font-size:13.5px;font-weight:600;color:var(--fg)}
h3.sub:first-child{margin-top:4px}
.lim{display:flex;align-items:center;gap:7px;flex-wrap:wrap;margin:5px 0 0}
.lim span{color:var(--mut);font-size:13.5px}
.lim input[type=number]{width:82px;flex:none;padding:7px 9px}
.gate{transition:opacity .15s}
.gate.off{opacity:.4}
#toast{position:fixed;left:50%;transform:translateX(-50%);bottom:18px;background:#173a24;
 color:var(--ok);padding:9px 18px;border-radius:99px;font-size:13.5px;opacity:0;
 transition:opacity .25s;pointer-events:none;border:1px solid #1f5231;z-index:9}
#toast.on{opacity:1}
</style>)CSS";

/*
 * Show/hide for password fields, shared by the setup page and the main page.
 * The wrapper and the button are built by script rather than written into the
 * markup: five fields across two pages would mean the same icon five times in
 * flash, and a plain <input> is what stays behind if the script ever fails.
 */
static const char PW_JS[] PROGMEM = R"JS(<script>
const EYE_ON='<svg viewBox="0 0 24 24"><path d="M1.7 12S5.3 5.6 12 5.6 22.3 12 22.3 12 18.7 18.4 12 18.4 1.7 12 1.7 12Z"/><circle cx=12 cy=12 r=3.1/></svg>';
const EYE_OFF='<svg viewBox="0 0 24 24"><path d="M9.9 5.9A9.6 9.6 0 0 1 12 5.6c6.7 0 10.3 6.4 10.3 6.4a18.6 18.6 0 0 1-3.7 4.4M6.2 7.6A18.6 18.6 0 0 0 1.7 12S5.3 18.4 12 18.4c1.4 0 2.7-.3 3.9-.7"/><path d="M9.8 9.9a3.1 3.1 0 0 0 4.3 4.3"/><path d="M3 3l18 18"/></svg>';
function pwLabel(shown){
 return LANG=='de' ? (shown?'Passwort verbergen':'Passwort anzeigen')
                   : (shown?'Hide password':'Show password');
}
// Called from applyLang() on both pages so the tooltip follows the language.
function pwLang(){
 document.querySelectorAll('.pw>button').forEach(b=>{
  const s=pwLabel(b.dataset.sh=='1'); b.title=s; b.setAttribute('aria-label',s);
 });
}
function pwEyes(){
 document.querySelectorAll('input[type=password]').forEach(inp=>{
  if(inp.parentNode.classList.contains('pw'))return;      // already wired
  const w=document.createElement('div'); w.className='pw';
  inp.parentNode.insertBefore(w,inp); w.appendChild(inp);
  const b=document.createElement('button');
  b.type='button'; b.dataset.sh='0'; b.innerHTML=EYE_ON;
  b.onclick=()=>{
   const show=inp.type=='password';
   inp.type=show?'text':'password';
   b.dataset.sh=show?'1':'0';
   b.innerHTML=show?EYE_OFF:EYE_ON;
   pwLang();
   inp.focus();
  };
  w.appendChild(b);
 });
 pwLang();
}
document.addEventListener('DOMContentLoaded',pwEyes);
</script>)JS";

static const char LOGO[] PROGMEM = R"LOGO(<svg viewBox="0 0 1015.5 207" xmlns="http://www.w3.org/2000/svg" fill=none class=logo role=img aria-label=owlanzi><g fill="#59E6CF"><rect x=11 y=0 width=9 height=9 rx=0.8 /><rect x=22 y=0 width=9 height=9 rx=0.8 /><rect x=154 y=0 width=9 height=9 rx=0.8 /><rect x=165 y=0 width=9 height=9 rx=0.8 /><rect x=11 y=11 width=9 height=9 rx=0.8 /><rect x=22 y=11 width=9 height=9 rx=0.8 /><rect x=33 y=11 width=9 height=9 rx=0.8 /><rect x=143 y=11 width=9 height=9 rx=0.8 /><rect x=154 y=11 width=9 height=9 rx=0.8 /><rect x=165 y=11 width=9 height=9 rx=0.8 /><rect x=22 y=22 width=9 height=9 rx=0.8 /><rect x=33 y=22 width=9 height=9 rx=0.8 /><rect x=44 y=22 width=9 height=9 rx=0.8 /><rect x=55 y=22 width=9 height=9 rx=0.8 /><rect x=66 y=22 width=9 height=9 rx=0.8 /><rect x=77 y=22 width=9 height=9 rx=0.8 /><rect x=88 y=22 width=9 height=9 rx=0.8 /><rect x=99 y=22 width=9 height=9 rx=0.8 /><rect x=110 y=22 width=9 height=9 rx=0.8 /><rect x=121 y=22 width=9 height=9 rx=0.8 /><rect x=132 y=22 width=9 height=9 rx=0.8 /><rect x=143 y=22 width=9 height=9 rx=0.8 /><rect x=154 y=22 width=9 height=9 rx=0.8 /><rect x=22 y=33 width=9 height=9 rx=0.8 /><rect x=33 y=33 width=9 height=9 rx=0.8 /><rect x=44 y=33 width=9 height=9 rx=0.8 /><rect x=55 y=33 width=9 height=9 rx=0.8 /><rect x=66 y=33 width=9 height=9 rx=0.8 /><rect x=77 y=33 width=9 height=9 rx=0.8 /><rect x=88 y=33 width=9 height=9 rx=0.8 /><rect x=99 y=33 width=9 height=9 rx=0.8 /><rect x=110 y=33 width=9 height=9 rx=0.8 /><rect x=121 y=33 width=9 height=9 rx=0.8 /><rect x=132 y=33 width=9 height=9 rx=0.8 /><rect x=143 y=33 width=9 height=9 rx=0.8 /><rect x=154 y=33 width=9 height=9 rx=0.8 /><rect x=11 y=44 width=9 height=9 rx=0.8 /><rect x=22 y=44 width=9 height=9 rx=0.8 /><rect x=77 y=44 width=9 height=9 rx=0.8 /><rect x=88 y=44 width=9 height=9 rx=0.8 /><rect x=99 y=44 width=9 height=9 rx=0.8 /><rect x=154 y=44 width=9 height=9 rx=0.8 /><rect x=165 y=44 width=9 height=9 rx=0.8 /><rect x=11 y=55 width=9 height=9 rx=0.8 /><rect x=88 y=55 width=9 height=9 rx=0.8 /><rect x=165 y=55 width=9 height=9 rx=0.8 /><rect x=0 y=66 width=9 height=9 rx=0.8 /><rect x=11 y=66 width=9 height=9 rx=0.8 /><rect x=88 y=66 width=9 height=9 rx=0.8 /><rect x=165 y=66 width=9 height=9 rx=0.8 /><rect x=176 y=66 width=9 height=9 rx=0.8 /><rect x=0 y=77 width=9 height=9 rx=0.8 /><rect x=11 y=77 width=9 height=9 rx=0.8 /><rect x=165 y=77 width=9 height=9 rx=0.8 /><rect x=176 y=77 width=9 height=9 rx=0.8 /><rect x=0 y=88 width=9 height=9 rx=0.8 /><rect x=11 y=88 width=9 height=9 rx=0.8 /><rect x=88 y=88 width=9 height=9 rx=0.8 /><rect x=165 y=88 width=9 height=9 rx=0.8 /><rect x=176 y=88 width=9 height=9 rx=0.8 /><rect x=0 y=99 width=9 height=9 rx=0.8 /><rect x=11 y=99 width=9 height=9 rx=0.8 /><rect x=77 y=99 width=9 height=9 rx=0.8 /><rect x=88 y=99 width=9 height=9 rx=0.8 /><rect x=99 y=99 width=9 height=9 rx=0.8 /><rect x=165 y=99 width=9 height=9 rx=0.8 /><rect x=176 y=99 width=9 height=9 rx=0.8 /><rect x=11 y=110 width=9 height=9 rx=0.8 /><rect x=22 y=110 width=9 height=9 rx=0.8 /><rect x=88 y=110 width=9 height=9 rx=0.8 /><rect x=154 y=110 width=9 height=9 rx=0.8 /><rect x=165 y=110 width=9 height=9 rx=0.8 /><rect x=0 y=121 width=9 height=9 rx=0.8 /><rect x=33 y=121 width=9 height=9 rx=0.8 /><rect x=44 y=121 width=9 height=9 rx=0.8 /><rect x=55 y=121 width=9 height=9 rx=0.8 /><rect x=66 y=121 width=9 height=9 rx=0.8 /><rect x=110 y=121 width=9 height=9 rx=0.8 /><rect x=121 y=121 width=9 height=9 rx=0.8 /><rect x=132 y=121 width=9 height=9 rx=0.8 /><rect x=143 y=121 width=9 height=9 rx=0.8 /><rect x=176 y=121 width=9 height=9 rx=0.8 /><rect x=0 y=132 width=9 height=9 rx=0.8 /><rect x=11 y=132 width=9 height=9 rx=0.8 /><rect x=44 y=132 width=9 height=9 rx=0.8 /><rect x=55 y=132 width=9 height=9 rx=0.8 /><rect x=66 y=132 width=9 height=9 rx=0.8 /><rect x=77 y=132 width=9 height=9 rx=0.8 /><rect x=88 y=132 width=9 height=9 rx=0.8 /><rect x=99 y=132 width=9 height=9 rx=0.8 /><rect x=110 y=132 width=9 height=9 rx=0.8 /><rect x=121 y=132 width=9 height=9 rx=0.8 /><rect x=132 y=132 width=9 height=9 rx=0.8 /><rect x=165 y=132 width=9 height=9 rx=0.8 /><rect x=176 y=132 width=9 height=9 rx=0.8 /><rect x=0 y=143 width=9 height=9 rx=0.8 /><rect x=11 y=143 width=9 height=9 rx=0.8 /><rect x=22 y=143 width=9 height=9 rx=0.8 /><rect x=33 y=143 width=9 height=9 rx=0.8 /><rect x=55 y=143 width=9 height=9 rx=0.8 /><rect x=66 y=143 width=9 height=9 rx=0.8 /><rect x=77 y=143 width=9 height=9 rx=0.8 /><rect x=88 y=143 width=9 height=9 rx=0.8 /><rect x=99 y=143 width=9 height=9 rx=0.8 /><rect x=110 y=143 width=9 height=9 rx=0.8 /><rect x=121 y=143 width=9 height=9 rx=0.8 /><rect x=143 y=143 width=9 height=9 rx=0.8 /><rect x=154 y=143 width=9 height=9 rx=0.8 /><rect x=165 y=143 width=9 height=9 rx=0.8 /><rect x=176 y=143 width=9 height=9 rx=0.8 /><rect x=11 y=154 width=9 height=9 rx=0.8 /><rect x=22 y=154 width=9 height=9 rx=0.8 /><rect x=33 y=154 width=9 height=9 rx=0.8 /><rect x=44 y=154 width=9 height=9 rx=0.8 /><rect x=66 y=154 width=9 height=9 rx=0.8 /><rect x=77 y=154 width=9 height=9 rx=0.8 /><rect x=88 y=154 width=9 height=9 rx=0.8 /><rect x=99 y=154 width=9 height=9 rx=0.8 /><rect x=110 y=154 width=9 height=9 rx=0.8 /><rect x=132 y=154 width=9 height=9 rx=0.8 /><rect x=143 y=154 width=9 height=9 rx=0.8 /><rect x=154 y=154 width=9 height=9 rx=0.8 /><rect x=165 y=154 width=9 height=9 rx=0.8 /><rect x=11 y=165 width=9 height=9 rx=0.8 /><rect x=22 y=165 width=9 height=9 rx=0.8 /><rect x=33 y=165 width=9 height=9 rx=0.8 /><rect x=44 y=165 width=9 height=9 rx=0.8 /><rect x=66 y=165 width=9 height=9 rx=0.8 /><rect x=77 y=165 width=9 height=9 rx=0.8 /><rect x=88 y=165 width=9 height=9 rx=0.8 /><rect x=99 y=165 width=9 height=9 rx=0.8 /><rect x=110 y=165 width=9 height=9 rx=0.8 /><rect x=132 y=165 width=9 height=9 rx=0.8 /><rect x=143 y=165 width=9 height=9 rx=0.8 /><rect x=154 y=165 width=9 height=9 rx=0.8 /><rect x=165 y=165 width=9 height=9 rx=0.8 /><rect x=22 y=176 width=9 height=9 rx=0.8 /><rect x=33 y=176 width=9 height=9 rx=0.8 /><rect x=44 y=176 width=9 height=9 rx=0.8 /><rect x=66 y=176 width=9 height=9 rx=0.8 /><rect x=77 y=176 width=9 height=9 rx=0.8 /><rect x=88 y=176 width=9 height=9 rx=0.8 /><rect x=99 y=176 width=9 height=9 rx=0.8 /><rect x=110 y=176 width=9 height=9 rx=0.8 /><rect x=132 y=176 width=9 height=9 rx=0.8 /><rect x=143 y=176 width=9 height=9 rx=0.8 /><rect x=154 y=176 width=9 height=9 rx=0.8 /><rect x=33 y=187 width=9 height=9 rx=0.8 /><rect x=44 y=187 width=9 height=9 rx=0.8 /><rect x=66 y=187 width=9 height=9 rx=0.8 /><rect x=77 y=187 width=9 height=9 rx=0.8 /><rect x=88 y=187 width=9 height=9 rx=0.8 /><rect x=99 y=187 width=9 height=9 rx=0.8 /><rect x=110 y=187 width=9 height=9 rx=0.8 /><rect x=132 y=187 width=9 height=9 rx=0.8 /><rect x=143 y=187 width=9 height=9 rx=0.8 /><rect x=66 y=198 width=9 height=9 rx=0.8 /><rect x=77 y=198 width=9 height=9 rx=0.8 /><rect x=88 y=198 width=9 height=9 rx=0.8 /><rect x=99 y=198 width=9 height=9 rx=0.8 /><rect x=110 y=198 width=9 height=9 rx=0.8 /><rect x=44 y=66 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=55 y=66 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=121 y=66 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=132 y=66 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=33 y=77 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=66 y=77 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=110 y=77 width=9 height=9 rx=0.8 fill="#B7B5C9" /><rect x=143 y=77 width=9 height=9 rx=0.8 fill="#B7B5C9" /></g><g id="wordmark" transform="translate(244 0)" stroke="#F5F4FA" stroke-width="29" stroke-linecap="round" stroke-linejoin="round"><circle id="o" cx="61" cy="146" r="46.5"/><circle id="a-bowl" cx="414" cy="146" r="46.5"/><path id="w" d="M150 99.5L150 160.5C150 178.173 164.327 192.5 182 192.5C199.673 192.5 214 178.173 214 160.5L214 112L214 160.5C214 178.173 228.327 192.5 246 192.5C263.673 192.5 278 178.173 278 160.5L278 99.5"/><path id="l" d="M322 59.5L322 192.5"/><path id="a-stem" d="M460.5 146L460.5 192.5"/><path id="n" d="M505 192.5L505 141.25C505 118.192 523.692 99.5 546.75 99.5C569.808 99.5 588.5 118.192 588.5 141.25L588.5 192.5"/><path id="z" d="M633 99.5L710 99.5L633 192.5L710 192.5"/><path id="i" d="M757 103L757 192.5"/><rect id="i-dot" x="742.5" y="50" width="29" height="27" rx="9" fill="#F277B7" stroke="none"/></g></svg>)LOGO";

/*
 * Tab icon: the pixel owl from the logo above, without the wordmark - at
 * 16 px a wordmark is a smear anyway. Same artwork, but on a 17x19 grid
 * instead of the logo's 11-px pitch and with each row's pixels merged into
 * runs, which turns 169 rects into 55 and the whole icon into under 2 kB.
 * Generated by tools/favicon.js out of the LOGO literal, so the icon cannot
 * drift away from the mark it is cut from.
 */
static const char FAVICON[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="-1 -1 19 21" shape-rendering=crispEdges><g fill="#59E6CF"><rect x=1 y=0 width=2 height=1/><rect x=14 y=0 width=2 height=1/><rect x=1 y=1 width=3 height=1/><rect x=13 y=1 width=3 height=1/><rect x=2 y=2 width=13 height=1/><rect x=2 y=3 width=13 height=1/><rect x=1 y=4 width=2 height=1/><rect x=7 y=4 width=3 height=1/><rect x=14 y=4 width=2 height=1/><rect x=1 y=5 width=1 height=1/><rect x=8 y=5 width=1 height=1/><rect x=15 y=5 width=1 height=1/><rect x=0 y=6 width=2 height=1/><rect x=8 y=6 width=1 height=1/><rect x=15 y=6 width=2 height=1/><rect x=0 y=7 width=2 height=1/><rect x=15 y=7 width=2 height=1/><rect x=0 y=8 width=2 height=1/><rect x=8 y=8 width=1 height=1/><rect x=15 y=8 width=2 height=1/><rect x=0 y=9 width=2 height=1/><rect x=7 y=9 width=3 height=1/><rect x=15 y=9 width=2 height=1/><rect x=1 y=10 width=2 height=1/><rect x=8 y=10 width=1 height=1/><rect x=14 y=10 width=2 height=1/><rect x=0 y=11 width=1 height=1/><rect x=3 y=11 width=4 height=1/><rect x=10 y=11 width=4 height=1/><rect x=16 y=11 width=1 height=1/><rect x=0 y=12 width=2 height=1/><rect x=4 y=12 width=9 height=1/><rect x=15 y=12 width=2 height=1/><rect x=0 y=13 width=4 height=1/><rect x=5 y=13 width=7 height=1/><rect x=13 y=13 width=4 height=1/><rect x=1 y=14 width=4 height=1/><rect x=6 y=14 width=5 height=1/><rect x=12 y=14 width=4 height=1/><rect x=1 y=15 width=4 height=1/><rect x=6 y=15 width=5 height=1/><rect x=12 y=15 width=4 height=1/><rect x=2 y=16 width=3 height=1/><rect x=6 y=16 width=5 height=1/><rect x=12 y=16 width=3 height=1/><rect x=3 y=17 width=2 height=1/><rect x=6 y=17 width=5 height=1/><rect x=12 y=17 width=2 height=1/><rect x=6 y=18 width=5 height=1/></g><g fill="#B7B5C9"><rect x=4 y=6 width=2 height=1/><rect x=11 y=6 width=2 height=1/><rect x=3 y=7 width=1 height=1/><rect x=6 y=7 width=1 height=1/><rect x=10 y=7 width=1 height=1/><rect x=13 y=7 width=1 height=1/></g></svg>)SVG";

// --- setup page (hotspot mode only) ----------------------------------------
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>owlanzi setup</title>
<link rel=icon href=/favicon.svg>
)HTML";

static const char SETUP_BODY[] PROGMEM = R"HTML(</h1>
 <span class=lang><button data-l=en class=on>EN</button><button data-l=de>DE</button></span></div>
<p class=note style=margin-bottom:18px data-i18n=setupIntro>Two things and you are done.
 The clock will restart and then show its IP address for 15 seconds.</p>

<div class=card>
 <div class=step><b>1</b><h2 data-i18n=wifi>Wi-Fi</h2></div>
 <button class="b g" onclick=scan() data-i18n=scanBtn>Scan for networks</button>
 <div id=nets></div>
 <label data-i18n=netName>Network name</label><input type=text id=wifiSsid autocapitalize=off>
 <label data-i18n=password>Password</label><input type=password id=wifiPass>
</div>

<div class=card>
 <div class=step><b>2</b><h2 data-i18n=owletAcc>Owlet account</h2></div>
 <label data-i18n=email>Email</label><input type=text id=owletMail autocapitalize=off>
 <label data-i18n=password>Password</label><input type=password id=owletPass>
 <label data-i18n=region>Region</label>
 <select id=europe><option value=1 data-i18n=europe>Europe</option>
  <option value=0 data-i18n=world>Rest of the world</option></select>
 <p class=note data-i18n=credNote>Your credentials stay on this device. The connection to
  the Owlet cloud verifies certificates.</p>
</div>

<div class=warnbox data-i18n=safety><b>This is an extra display.</b> It complements the Owlet
 base station and does not replace it. Wi-Fi can drop, the Owlet cloud can fail, this
 firmware can have bugs. Keep relying on the base station.</div>

<button class="b big" onclick=saveSetup() data-i18n=saveRestart>Save and restart</button>
<div id=toast></div>
</div>
<script>
const $=i=>document.getElementById(i);
const esc=s=>String(s).replace(/[<>&"]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]));
const DE={setupIntro:'Zwei Angaben, dann ist es geschafft. Die Uhr startet neu und zeigt danach 15 Sekunden lang ihre IP-Adresse.',
 wifi:'WLAN',scanBtn:'Netzwerke suchen',netName:'Netzname',password:'Passwort',
 owletAcc:'Owlet-Konto',email:'E-Mail',region:'Region',europe:'Europa',world:'Rest der Welt',
 credNote:'Die Zugangsdaten bleiben auf diesem Gerät. Die Verbindung zur Owlet-Cloud prüft Zertifikate.',
 safety:'<b>Dies ist eine Zusatzanzeige.</b> Sie ergänzt die Owlet-Basisstation und ersetzt sie nicht. WLAN kann ausfallen, die Owlet-Cloud kann stören, diese Firmware kann Fehler haben. Verlass dich weiterhin auf die Basisstation.',
 saveRestart:'Speichern und neu starten',searching:'searching ...',
 noNets:'nothing found, try again',scanFail:'scan failed'};
const DE2={searching:'suche ...',noNets:'nichts gefunden, nochmal versuchen',scanFail:'Suche fehlgeschlagen'};
let EN={};
function applyLang(l){
 document.querySelectorAll('[data-i18n]').forEach(e=>{
  const k=e.dataset.i18n;
  if(EN[k]===undefined)EN[k]=e.innerHTML;
  e.innerHTML = (l=='de'&&DE[k]!==undefined)?DE[k]:EN[k];
 });
 document.querySelectorAll('.lang button').forEach(b=>b.classList.toggle('on',b.dataset.l==l));
 document.documentElement.lang=l; localStorage.setItem('owlLang',l); LANG=l;
 if(typeof pwLang=='function')pwLang();
}
let LANG='en';
document.querySelectorAll('.lang button').forEach(b=>b.onclick=()=>applyLang(b.dataset.l));
function t(k,en){ return LANG=='de'&&DE2[k]?DE2[k]:en; }

async function scan(){
 const d=$('nets'); d.innerHTML='<p class=note>'+t('searching','searching ...')+'</p>';
 try{
  const l=await(await fetch('/api/scan')).json();
  if(!l.length){d.innerHTML='<p class=note>'+t('noNets','nothing found, try again')+'</p>';return;}
  d.innerHTML=l.map(n=>'<div class=sw style=cursor:pointer onclick="pick(this)" data-s="'+
   esc(n.s)+'"><span>'+esc(n.s)+(n.e?' &#128274;':'')+'</span><small style=color:var(--mut)>'+
   n.r+' dBm</small></div>').join('');
 }catch(e){ d.innerHTML='<p class=note>'+t('scanFail','scan failed')+'</p>'; }
}
function pick(el){ $('wifiSsid').value=el.dataset.s; $('wifiPass').focus(); }

async function saveSetup(){
 if(!$('wifiSsid').value){ alert(LANG=='de'?'Bitte ein WLAN angeben.':'Please enter a network name.'); return; }
 const b={wifiSsid:$('wifiSsid').value,wifiPass:$('wifiPass').value,
  owletMail:$('owletMail').value,owletPass:$('owletPass').value,
  europe:$('europe').value=='1',lang:LANG};
 await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},
   body:JSON.stringify(b)});
 document.body.innerHTML='<div style="max-width:520px;margin:70px auto;padding:24px;'+
  'font:16px/1.6 system-ui;color:#e8ebf0">'+
  (LANG=='de'
   ? '<h2 style=margin-top:0>Gespeichert</h2><p>Die Uhr startet jetzt neu und verbindet sich.</p>'+
     '<p style=color:#8d97a6>Sobald sie im Netz ist, zeigt sie 15 Sekunden lang ihre IP-Adresse. '+
     'Unter dieser Adresse erreichst du sie danach im Browser.</p>'+
     '<p style=color:#8d97a6>Klappt die Verbindung nicht, meldet sie WLAN FEHLER und öffnet '+
     'wieder den Hotspot <b>owlanzi</b>.</p>'
   : '<h2 style=margin-top:0>Saved</h2><p>The clock is restarting and connecting now.</p>'+
     '<p style=color:#8d97a6>Once it joins the network it shows its IP address for 15 seconds. '+
     'That is where you reach it in a browser from now on.</p>'+
     '<p style=color:#8d97a6>If it cannot connect it shows WIFI ERROR and reopens the '+
     '<b>owlanzi</b> hotspot.</p>')+'</div>';
}
applyLang(localStorage.getItem('owlLang')||'en');
</script>
)HTML";

// --- Hauptseite ------------------------------------------------------------
static const char P_HEAD[] PROGMEM = R"HTML(<!doctype html><html lang=en><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>owlanzi</title>
<link rel=icon href=/favicon.svg>
)HTML";

static const char P_A[] PROGMEM = R"HTML(
<style>header{position:sticky;top:0;z-index:5;background:linear-gradient(180deg,var(--bg) 72%,transparent);padding:14px 0 10px}
.pill{font-size:12px;padding:3px 10px;border-radius:99px;background:#1e2530;color:var(--mut)}
.pill.ok{background:#10321f;color:var(--ok)}.pill.bad{background:#3a1b1b;color:var(--bad)}
.pill.warn{background:#3a2f14;color:var(--warn)}
#mx{width:100%;display:block;border-radius:10px;background:#05060a;border:1px solid var(--line)}
.mxbar{display:flex;align-items:center;gap:10px;margin-top:7px;font-size:12.5px;color:var(--mut)}
.mxbar label{display:flex;align-items:center;gap:5px;cursor:pointer;margin:0}
nav{display:flex;gap:4px;margin:14px 0 4px;background:#12161d;padding:4px;border-radius:10px}
nav button{flex:1;padding:8px 4px;background:none;border:0;color:var(--mut);border-radius:7px;
 font-size:13.5px;font-weight:550;cursor:pointer}
nav button.on{background:#232b38;color:var(--fg)}
section{display:none}section.on{display:block}
.big{display:flex;gap:9px;margin:2px 0 4px}
.big>div{flex:1;background:#1a2029;border-radius:10px;padding:12px 8px;text-align:center}
.big b{display:block;font-size:26px;font-weight:640;line-height:1.15}
.big small{color:var(--mut);font-size:11.5px}
.alarmbox{background:#2c1414;border:1px solid #5a2222;border-radius:9px;padding:12px;margin:10px 0}
.alarmbox b{color:var(--bad)}
</style><div class=w>
<header>
 <div class=title><h1>)HTML";

static const char P_B[] PROGMEM = R"HTML(</h1>
  <span class=pill id=pScreen style=margin-left:auto>&mdash;</span>
  <span class=lang><button data-l=en class=on>EN</button><button data-l=de>DE</button></span></div>
 <canvas id=mx width=640 height=160></canvas>
 <div class=mxbar><span id=mxInfo data-i18n=liveMirror>Live mirror of the matrix</span>
  <label style=margin-left:auto><input type=checkbox id=simBri>
   <span data-i18n=simBri>simulate brightness</span></label></div>
 <nav><button class=on data-t=0 data-i18n=tStatus>Status</button>
  <button data-t=1 data-i18n=tDisplay>Display</button>
  <button data-t=2 data-i18n=tAlarms>Alarms</button>
  <button data-t=3 data-i18n=tSystem>System</button></nav>
</header>

<section class=on id=s0>
 <div id=alarmHere></div>
 <div class=big><div><b id=vHr>--</b><small data-i18n=uHr>heart rate bpm</small></div>
  <div><b id=vOx>--</b><small data-i18n=uOx>oxygen %</small></div>
  <div><b id=vBat>--</b><small data-i18n=uBat>sock battery %</small></div></div>
 <div class=card><h2 data-i18n=hShowing>What the clock is showing</h2>
  <div class=row><span data-i18n=fScreen>Screen</span><span id=dScreen>-</span></div>
  <div class=row><span data-i18n=fWhy>Reason</span><span id=dWhy>-</span></div>
  <div class=row><span data-i18n=fBri>Brightness</span><span id=dBri>-</span></div>
  <div class=row><span data-i18n=fLight>Ambient light</span><span id=dLdr>-</span></div>
 </div>
 <div class=card><h2 data-i18n=hSock>Sock</h2>
  <div class=row><span data-i18n=fSleep>Sleep state</span><span id=dSleep>-</span></div>
  <div class=row><span data-i18n=fCharge>Charging</span><span id=dChg>-</span></div>
  <div class=row><span data-i18n=fSockOff>Sock removed</span><span id=dOff>-</span></div>
  <div class=row><span data-i18n=fBase>Base station on</span><span id=dBso>-</span></div>
  <div class=row><span data-i18n=fHw>Hardware</span><span id=dHw>-</span></div>
 </div>
 <div class=card><h2 data-i18n=hConn>Connection</h2>
  <div class=row><span data-i18n=fWifi>Wi-Fi</span><span id=dWifi>-</span></div>
  <div class=row><span data-i18n=fOwlet>Owlet</span><span id=dLogin>-</span></div>
  <div class=row><span data-i18n=fLast>Last fetch</span><span id=dAge>-</span></div>
  <div class=row><span data-i18n=fCnt>Fetches / failures</span><span id=dCnt>-</span></div>
  <div class=row><span data-i18n=fTok>Token expires in</span><span id=dTok>-</span></div>
  <div class=row><span data-i18n=fHeap>Free memory</span><span id=dHeap>-</span></div>
  <div class=row><span data-i18n=fUp>Uptime</span><span id=dUp>-</span></div>
  <div id=errRow></div>
 </div>
</section>
)HTML";

static const char P_C[] PROGMEM = R"HTML(
<section id=s1>
 <div class=card><h2 data-i18n=hPreview>Preview a screen</h2>
  <p class=note data-i18n=previewNote>Shows the screen with made-up values, on the clock and
   in the mirror above. While you change colours below, the clock follows along; it returns
   to the normal display ten seconds after the last change, when you save, or when you press
   Back to normal. Nothing is stored until you save.</p>
  <button class="b g" onclick=t(4) data-i18n=scVitals>Vitals</button>
  <button class="b g" onclick=t(5) data-i18n=scBattery>Battery</button>
  <button class="b g" onclick=t(6) data-i18n=scWaiting>Waiting</button>
  <button class="b g" onclick=t(7)>Offline</button>
  <button class="b g" onclick=t(8) data-i18n=scAlarm>Alarm</button>
  <button class="b g" onclick=t(9) data-i18n=scInfo>Notice</button>
  <button class=b onclick=t(0) data-i18n=scBack>Back to normal</button>
  <label data-i18n=lTest>Brightness while previewing</label>
  <div class=sl><input type=range id=briTest min=1 max=255><output></output></div>
  <p class=note data-i18n=testBriNote>Takes effect on the clock at once, so a colour can be
   judged at the brightness it will actually be seen at. Saved along with everything else.</p>
 </div>
 <div class=card><h2 data-i18n=hVitals>Vitals</h2>
  <canvas class=pv id=pvVit width=480 height=120></canvas>
  <p class=pvcap data-i18n=pvCap>Preview with made-up values. Pick a colour below and the
   preview switches to the case it belongs to.</p>
  <div class=sw><input type=color id=c_heart data-pv=vitals><span data-i18n=cHeart>Heart</span></div>
  <div class=sw><input type=color id=c_numbers data-pv=vitals><span data-i18n=cNumbers>Numbers</span></div>
  <div class=sw><input type=color id=c_sep data-pv=vitals><span data-i18n=cSep>Separator dot</span></div>
 </div>
 <div class=card><h2 data-i18n=hSleepBar>Sleep bar</h2>
  <canvas class=pv id=pvSleep width=480 height=120></canvas>
  <p class=pvcap data-i18n=pvSleepCap>The bar sits in the bottom row of the vitals screen.
   The deeper the sleep, the shorter it gets.</p>
  <div class=sw><input type=color id=c_awake data-pv=sleep1><span data-i18n=cAwake>awake</span></div>
  <div class=sw><input type=color id=c_lightSleep data-pv=sleep2><span data-i18n=cLight>light sleep</span></div>
  <div class=sw><input type=color id=c_deepSleep data-pv=sleep3><span data-i18n=cDeep>deep sleep</span></div>
  <div class=sw><input type=color id=c_sleepUnk data-pv=sleep0><span data-i18n=cUnk>unknown</span></div>
  <p class=note data-i18n=sleepNote>The numeric sleep-state values are undocumented. Anything
   unknown deliberately stays grey rather than faking a sleep depth.</p>
 </div>
 <div class=card><h2 data-i18n=hBattery>Battery</h2>
  <canvas class=pv id=pvBat width=480 height=120></canvas>
  <p class=pvcap data-i18n=pvCap>Preview with made-up values. Pick a colour below and the
   preview switches to the case it belongs to.</p>
  <div class=sw><input type=color id=c_batFrame data-pv=bat0><span data-i18n=cFrame>outline</span></div>
  <div class=sw><input type=color id=c_batOk data-pv=bat0><span data-i18n=cFill>fill, normal</span></div>
  <div class=sw><input type=color id=c_batCharge data-pv=bat1><span data-i18n=cCharge>fill, charging</span></div>
  <div class=sw><input type=color id=c_batMid data-pv=bat2><span data-i18n=cMid>fill, below 40 %</span></div>
  <div class=sw><input type=color id=c_batLow data-pv=bat3><span data-i18n=cLow>fill, below 20 %</span></div>
  <div class=sw><input type=color id=c_batText data-pv=bat0><span data-i18n=cBatText>percentage</span></div>
 </div>
 <div class=card><h2 data-i18n=hWaitOff>Waiting and offline</h2>
  <canvas class=pv id=pvWait width=480 height=120></canvas>
  <p class=pvcap data-i18n=pvCap>Preview with made-up values. Pick a colour below and the
   preview switches to the case it belongs to.</p>
  <div class=sw><input type=color id=c_heartWait data-pv=wait><span data-i18n=cHeartW>heart, no fresh values</span></div>
  <div class=sw><input type=color id=c_dashes data-pv=wait><span data-i18n=cDash>dashes</span></div>
  <div class=sw><input type=color id=c_offline data-pv=offline><span>OFFLINE</span></div>
 </div>
 <div class=card><h2 data-i18n=hAlarmCol>Alarm colours</h2>
  <canvas class=pv id=pvAlarm width=480 height=120></canvas>
  <p class=pvcap data-i18n=pvAlarmCap>On the clock the text scrolls and the three corner
   pixels blink; the preview holds it still.</p>
  <div class=sw><input type=color id=c_alarm data-pv=alarm><span data-i18n=cAlarm>critical alarm</span></div>
  <div class=sw><input type=color id=c_info data-pv=info><span data-i18n=cInfo>notice, e.g. battery low</span></div>
 </div>
 <div class=card><h2 data-i18n=hBright>Brightness</h2>
  <label data-i18n=lMin>Minimum &ndash; the normal case, and what the clock shows nearly all night</label>
  <div class=sl><input type=range id=briMin min=1 max=255><output></output></div>
  <div id=briWarn class=warnbox hidden data-i18n=briWarnTxt><b>Below step 5 colours are no
   longer shown faithfully.</b> A grey pixel lights all three of its LEDs, a saturated one only
   a single LED, so reds and blues drop out first while greys are still visible - and some
   pixels stop lighting at all. Fine if a night has to be truly dark; just do not judge a
   colour down here.</div>
  <label data-i18n=lDay>Bright &ndash; only when the sock is active AND the room is bright</label>
  <div class=sl><input type=range id=briDay min=1 max=255><output></output></div>
  <label data-i18n=lAlarm>During an alarm</label>
  <div class=sl><input type=range id=briAlarm min=1 max=255><output></output></div>
  <p class=note data-i18n=briPrevNote>The brightness for previews sits at the top, next to
   the preview buttons - that is where it is needed.</p>
  <label><span data-i18n=lThresh>Ambient light counts as bright above</span>
   (<span data-i18n=lNow>now</span>: <span id=ldrNow>?</span>)</label>
  <div class=sl><input type=range id=ldrThreshold min=0 max=1023><output></output></div>
  <label data-i18n=lHyst>Hysteresis &ndash; stops flicker around the threshold</label>
  <div class=sl><input type=range id=ldrHysteresis min=0 max=200><output></output></div>
 </div>
 <div class=card><h2 data-i18n=hPanel>Check the panel</h2>
  <p class=note data-i18n=panelNote>Corners lights four pixels: top left <b style=color:#f66>red</b>,
   top right <b style=color:#6f6>green</b>, bottom left <b style=color:#66f>blue</b>, bottom right
   white. Chase walks one pixel through all 256 in order, Colours fills the whole matrix. Between
   them they show up a dead pixel or a bad joint.</p>
  <button class="b g" onclick=t(1) data-i18n=scCorners>Corners</button>
  <button class="b g" onclick=t(2) data-i18n=scSweep>Chase</button>
  <button class="b g" onclick=t(3) data-i18n=scColors>Colours</button>
 </div>
 <button class=b onclick=save() data-i18n=saveDisplay>Save display</button>
 <button class="b g" onclick=resetAll() data-i18n=resetCols>All colours to default</button>
</section>

<section id=s2>
 <div class=warnbox data-i18n=ownWarn>These alarms come from <b>this device</b>, not from Owlet.
  They complement the base station and do not replace it. A reading of 0 never triggers &ndash;
  that is the station being switched off, not a vital sign.</div>
 <div class=card><h2 data-i18n=hOwn>Own alarms</h2>
  <div class=chk><input type=checkbox id=ownAlarms><label style=margin:0 data-i18n=lArm>arm them</label></div>
  <p class=note style=margin-top:0 data-i18n=armNote>Off means this device stays quiet. The
   base station is unaffected either way.</p>
  <div class=gate id=gArm>
   <h3 class=sub data-i18n=sOx>Oxygen</h3>
   <div class=lim><span data-i18n=wBelow>alarm below</span>
    <input type=number id=spo2Limit min=50 max=99><span>%</span>
    <span data-i18n=wFor>held for</span>
    <input type=number id=spo2Seconds min=5 max=300><span data-i18n=wSec>s</span></div>
   <h3 class=sub data-i18n=sHrLow>Heart rate too low</h3>
   <div class=lim><span data-i18n=wBelow>alarm below</span>
    <input type=number id=hrLowLimit min=30 max=150><span data-i18n=wBpm>bpm</span>
    <span data-i18n=wFor>held for</span>
    <input type=number id=hrLowSeconds min=5 max=300><span data-i18n=wSec>s</span></div>
   <h3 class=sub data-i18n=sHrHigh>Heart rate too high</h3>
   <div class=lim><span data-i18n=wAbove>alarm above</span>
    <input type=number id=hrHighLimit min=100 max=260><span data-i18n=wBpm>bpm</span>
    <span data-i18n=wFor>held for</span>
    <input type=number id=hrHighSeconds min=5 max=300><span data-i18n=wSec>s</span></div>
   <p class=note data-i18n=durNote>Duration rather than a count of readings: a steadily bad
    value is the most dangerous case and must not slip through a counter.</p>
  </div>
 </div>
 <div class=card><h2 data-i18n=hSound>Sound</h2>
  <div class=chk><input type=checkbox id=soundEnabled><label style=margin:0 data-i18n=lSound>allow sound</label></div>
  <p class=note style=margin-top:0 data-i18n=soundNote>Off means an alarm only shows on the
   matrix. Nothing here makes a sound on its own.</p>
  <div class=gate id=gSound>
   <label data-i18n=lVol>Volume (0&ndash;30)</label>
   <div class=sl><input type=range id=volAlarm min=0 max=30><output></output></div>
   <label data-i18n=lRepeat>Repeat the alarm tone every (s, 0 = once)</label>
   <div class=sl><input type=range id=alarmRepeatSec min=0 max=120><output></output></div>
   <h3 class=sub data-i18n=sTest>Try it out</h3>
   <p class=note style=margin-top:2px data-i18n=soundWarn><b>Careful:</b> these play out loud.
    Not next to a sleeping child.</p>
   <button class="b g" onclick=snd('beep') data-i18n=bBeep>Short beep</button>
   <button class="b r" onclick=snd('alarm') data-i18n=bAlarm>Alarm melody</button>
  </div>
 </div>
 <button class=b onclick=save() data-i18n=saveAlarms>Save alarms</button>
</section>
)HTML";

static const char P_D[] PROGMEM = R"HTML(
<section id=s3>
 <div class=card><h2 data-i18n=hLang>Language</h2>
  <p class=note data-i18n=langNote>Applies to this interface and to the text on the clock.
   The setting is stored on the device.</p>
  <span class=lang style=display:inline-flex><button data-l=en>EN</button><button data-l=de>DE</button></span>
 </div>
 <div class=card><h2 data-i18n=wifi>Wi-Fi</h2>
  <label data-i18n=netName>Network name</label><input type=text id=wifiSsid autocapitalize=off>
  <button class="b g" style=margin-top:8px onclick=scan() data-i18n=scanBtn>Scan for networks</button>
  <div id=nets></div>
  <label data-i18n=password>Password</label><input type=password id=wifiPass data-i18n-ph=phKeep placeholder="leave empty to keep">
  <p class=note data-i18n=wifiNote>Changing the network restarts the device.</p>
 </div>
 <div class=card><h2 data-i18n=owletAcc>Owlet account</h2>
  <label data-i18n=email>Email</label><input type=text id=owletMail autocapitalize=off>
  <label data-i18n=password>Password</label><input type=password id=owletPass data-i18n-ph=phKeep placeholder="leave empty to keep">
  <label data-i18n=region>Region</label>
  <select id=europe><option value=1 data-i18n=europe>Europe</option>
   <option value=0 data-i18n=world>Rest of the world</option></select>
  <label data-i18n=lPoll>Fetch interval</label>
  <select id=pollSeconds><option value=5>5 s</option><option value=10>10 s</option>
   <option value=15>15 s</option></select>
  <p class=note data-i18n=pollNote>Shorter means fresher values and more radio traffic.</p>
  <p class=note data-i18n=credNote>Your credentials stay on this device. The connection to
   the Owlet cloud verifies certificates.</p>
 </div>
 <div class=card><h2 data-i18n=hAccess>Access to this interface</h2>
  <label data-i18n=lWebPass>Password (empty = no login)</label>
  <input type=password id=webPass data-i18n-ph=phKeep placeholder="leave empty to keep">
  <p class=note data-i18n=accessNote>Without a password anyone on the network can switch the
   alarms off. Recommended for a device you hand to someone else. The username is <b>owlanzi</b>.</p>
 </div>
 <div class=card><h2 data-i18n=hUpdate>Update firmware</h2>
  <p class=note data-i18n=updNote>Upload a new <b>owlanzi</b> firmware (.bin). It is written to
   the second program partition and the device restarts. If anything goes wrong the old one
   keeps running.</p>
  <input type=file id=fw accept=".bin">
  <button class=b onclick=ota() data-i18n=bUpload>Upload and install</button>
  <div id=otaProg class=note></div>
  <p class=note data-i18n=awtrixNote><b>Back to AWTRIX?</b> Not this way. An update replaces
   only the program, not the partition table &ndash; AWTRIX needs its own. Use the AWTRIX web
   flasher over USB: hold the middle button while powering on, then open
   <a href="https://awtrix.github.io/AWTRIX3/docs/ulanzi_flasher/" style=color:var(--acc)>the flasher</a>.</p>
 </div>
 <div class=card><h2 data-i18n=hFactory>Factory reset</h2>
  <p class=note data-i18n=factNote>Erases every setting including the credentials. The device
   then reopens the <b>owlanzi</b> hotspot.</p>
  <button class="b r" onclick=factory() data-i18n=bFactory>Reset to factory settings</button>
 </div>
 <button class=b onclick=save() data-i18n=saveSystem>Save system</button>
</section>
<div id=toast></div>
</div>
)HTML";

static const char P_JS1[] PROGMEM = R"HTML(<script>
const $=i=>document.getElementById(i);
const esc=s=>String(s).replace(/[<>&"]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]));
const DE={
liveMirror:'Live-Spiegel der Matrix',simBri:'Helligkeit simulieren',
tStatus:'Status',tDisplay:'Anzeige',tAlarms:'Alarme',tSystem:'System',
uHr:'Puls bpm',uOx:'Sauerstoff %',uBat:'Socke Akku %',
hShowing:'Was die Uhr gerade zeigt',fScreen:'Schirm',fWhy:'Grund',fBri:'Helligkeit',
fLight:'Umgebungslicht',hSock:'Socke',fSleep:'Schlafzustand',fCharge:'Laden',
fSockOff:'Socke abgelegt',fBase:'Basisstation an',fHw:'Hardware',
hConn:'Verbindung',fWifi:'WLAN',fOwlet:'Owlet',fLast:'Letzter Abruf',
fCnt:'Abrufe / Fehler',fTok:'Token läuft ab in',fHeap:'Speicher frei',fUp:'Laufzeit',
hPreview:'Schirm ansehen',
previewNote:'Zeigt den Schirm mit erfundenen Werten auf der Uhr und oben im Spiegel. Während du unten Farben änderst, zieht die Uhr mit; zehn Sekunden nach der letzten Änderung, beim Speichern oder mit "Zurück zur Normalanzeige" ist sie wieder normal. Gespeichert wird erst beim Speichern.',
testBriNote:'Wirkt sofort auf der Uhr, damit sich eine Farbe bei der Helligkeit beurteilen lässt, in der sie später auch zu sehen ist. Wird mitgespeichert.',
briPrevNote:'Die Helligkeit für Vorschauen steht oben bei den Vorschau-Knöpfen &ndash; dort wird sie gebraucht.',
scVitals:'Vitalwerte',scBattery:'Akku',scWaiting:'Warten',scAlarm:'Alarm',scInfo:'Hinweis',
scBack:'Zurück zur Normalanzeige',
hVitals:'Vitalwerte',cHeart:'Herz',cNumbers:'Zahlen',cSep:'Trennpunkt',
pvCap:'Vorschau mit erfundenen Werten. Wer unten eine Farbe anfasst, sieht oben den Fall dazu.',
pvSleepCap:'Der Balken liegt in der untersten Zeile der Vitalanzeige. Je tiefer der Schlaf, desto kürzer.',
pvAlarmCap:'Auf der Uhr läuft der Text durch und die drei Eckpixel blinken; die Vorschau hält ihn an.',
hSleepBar:'Schlafbalken',cAwake:'wach',cLight:'leichter Schlaf',cDeep:'Tiefschlaf',cUnk:'unbekannt',
sleepNote:'Die Zahlenwerte für den Schlafzustand sind nicht dokumentiert. Alles Unbekannte bleibt bewusst grau, statt eine Tiefe vorzutäuschen.',
hBattery:'Akku',cFrame:'Umriss',cFill:'Füllung normal',cCharge:'Füllung beim Laden',
cMid:'Füllung unter 40 %',cLow:'Füllung unter 20 %',cBatText:'Prozentzahl',
hWaitOff:'Warten und Offline',cHeartW:'Herz ohne frische Werte',cDash:'Striche',
hAlarmCol:'Alarmfarben',cAlarm:'kritischer Alarm',cInfo:'Hinweis, z. B. Akku leer',
hBright:'Helligkeit',lMin:'Minimal &ndash; der Normalfall, und das, was die Uhr fast die ganze Nacht zeigt',
briWarnTxt:'<b>Unter Stufe 5 werden Farben nicht mehr richtig dargestellt.</b> Ein grauer Pixel zündet alle drei LEDs, ein gesättigter nur eine &ndash; Rot und Blau fallen deshalb zuerst weg, während Grautöne noch da sind, und einzelne Pixel leuchten gar nicht mehr. Für eine Nacht, die wirklich dunkel sein soll, ist das in Ordnung; nur beurteilen lässt sich eine Farbe hier unten nicht.',
lDay:'Hell &ndash; nur wenn Socke aktiv UND Umgebung hell',lAlarm:'Bei Alarm',
lTest:'Helligkeit in der Vorschau',lThresh:'Ab diesem Umgebungslicht gilt hell',lNow:'jetzt',
lHyst:'Hysterese &ndash; verhindert Flackern an der Schwelle',
hPanel:'Panel prüfen',
panelNote:'Ecken zündet vier Pixel: oben links <b style=color:#f66>rot</b>, oben rechts <b style=color:#6f6>grün</b>, unten links <b style=color:#66f>blau</b>, unten rechts weiß. Lauflicht schickt einen Pixel der Reihe nach durch alle 256, Farben füllt die ganze Matrix. Zusammen zeigen sie einen toten Pixel oder eine kalte Lötstelle.',
scCorners:'Ecken',scSweep:'Lauflicht',scColors:'Farben',
saveDisplay:'Anzeige speichern',resetCols:'Alle Farben auf Original',
ownWarn:'Diese Alarme kommen von <b>diesem Gerät</b>, nicht von Owlet. Sie ergänzen die Basisstation und ersetzen sie nicht. Ein Messwert von 0 löst nie aus &ndash; das ist die abgeschaltete Station, kein Vitalwert.',
hOwn:'Eigene Alarme',lArm:'scharf schalten',
armNote:'Aus heißt: dieses Gerät bleibt still. Die Basisstation macht so oder so ihr eigenes Ding.',
sOx:'Sauerstoff',sHrLow:'Puls zu niedrig',sHrHigh:'Puls zu hoch',
wBelow:'Alarm unter',wAbove:'Alarm über',wFor:'für mindestens',wSec:'s',wBpm:'bpm',
durNote:'Dauer statt Anzahl der Messungen: ein stabil schlechter Wert ist der gefährlichste Fall und darf nicht durch eine Zählung fallen.',
hSound:'Ton',lSound:'Ton erlauben',
soundNote:'Aus heißt: ein Alarm erscheint nur auf der Matrix. Von allein gibt hier nichts einen Ton.',
sTest:'Ausprobieren',lVol:'Lautstärke (0&ndash;30)',
lRepeat:'Alarmton wiederholen alle (s, 0 = nur einmal)',
soundWarn:'<b>Achtung:</b> spielt hörbar ab. Nicht neben einem schlafenden Kind.',
bBeep:'Kurzer Piep',bAlarm:'Alarmmelodie',saveAlarms:'Alarme speichern',
hLang:'Sprache',langNote:'Gilt für diese Oberfläche und für den Text auf der Uhr. Die Einstellung liegt auf dem Gerät.',
wifi:'WLAN',netName:'Netzname',scanBtn:'Netzwerke suchen',password:'Passwort',
phKeep:'leer lassen = beibehalten',
wifiNote:'Nach einer WLAN-Änderung startet das Gerät neu.',
owletAcc:'Owlet-Konto',email:'E-Mail',region:'Region',europe:'Europa',world:'Rest der Welt',
lPoll:'Abrufabstand',
pollNote:'Kürzer heißt frischere Werte und mehr Funkverkehr.',
credNote:'Die Zugangsdaten bleiben auf diesem Gerät. Die Verbindung zur Owlet-Cloud prüft Zertifikate.',
hAccess:'Zugriff auf diese Oberfläche',lWebPass:'Passwort (leer = ohne Anmeldung)',
accessNote:'Ohne Passwort kann jeder im Netz die Alarme abschalten. Für ein Gerät, das andere bekommen, empfohlen. Benutzername ist <b>owlanzi</b>.',
hUpdate:'Firmware aktualisieren',
updNote:'Eine neue <b>owlanzi</b>-Firmware (.bin) hochladen. Sie wird in die zweite Programmpartition geschrieben, dann startet das Gerät neu. Geht etwas schief, läuft die alte Fassung weiter.',
bUpload:'Hochladen und einspielen',
awtrixNote:'<b>Zurück zu AWTRIX?</b> Nicht über diesen Weg. Ein Update ersetzt nur das Programm, nicht die Partitionstabelle &ndash; AWTRIX braucht seine eigene. Der sichere Weg ist der AWTRIX-Web-Flasher über USB: mittlere Taste beim Einschalten halten und <a href="https://awtrix.github.io/AWTRIX3/docs/ulanzi_flasher/" style=color:var(--acc)>den Flasher</a> öffnen.',
hFactory:'Auf Werk zurücksetzen',
factNote:'Löscht alle Einstellungen samt Zugangsdaten. Das Gerät öffnet danach wieder den Hotspot <b>owlanzi</b>.',
bFactory:'Auf Werk zurücksetzen',saveSystem:'System speichern'};
const T={saved:['saved','gespeichert'],savedReboot:['saved, device is restarting','gespeichert, Gerät startet neu'],
 colsReset:['colours reset &ndash; still needs saving','Farben zurückgesetzt &ndash; noch speichern'],
 searching:['searching ...','suche ...'],noNets:['nothing found, try again','nichts gefunden, nochmal versuchen'],
 scanFail:['scan failed','Suche fehlgeschlagen'],pickBin:['Pick a .bin file first.','Erst eine .bin auswählen.'],
 cfgFail:['could not load the settings &ndash; reload the page','Einstellungen nicht geladen &ndash; Seite neu laden'],
 uploading:['uploading ...','wird übertragen ...'],otaOk:['installed, device is restarting','eingespielt, Gerät startet neu'],
 otaFail:['failed: ','fehlgeschlagen: '],otaLost:['connection dropped (can happen on success)','Verbindung abgebrochen (kann bei Erfolg passieren)'],
 alarm:['ALARM','ALARM'],notice:['NOTICE','HINWEIS'],hush:['Acknowledge tone','Ton quittieren'],
 yes:['yes','ja'],no:['no','nein'],on:['connected','angemeldet'],off:['not connected','nicht angemeldet'],
 sAgo:[' s ago',' s her'],min:[' min',' min'],dark:[' (dark)',' (dunkel)'],bright:[' (bright)',' (hell)'],
 lastErr:['Last error','Letzter Fehler'],
 confFact:['Erase everything? Wi-Fi, Owlet account and all settings will be gone.',
           'Wirklich alles löschen? WLAN, Owlet-Konto und alle Einstellungen sind danach weg.'],
 confOta:['Install "%1" (%2 kB)? The device will restart.','"%1" (%2 kB) einspielen? Das Gerät startet danach neu.']};
let LANG='en', EN={};
const tr=k=>T[k][LANG=='de'?1:0];
</script>)HTML";

static const char P_JS2[] PROGMEM = R"HTML(<script>
const COLS=['heart','numbers','sep','heartWait','dashes','awake','lightSleep','deepSleep',
 'sleepUnk','batFrame','batOk','batCharge','batMid','batLow','batText','alarm','info','offline'];
const NUMS=['spo2Limit','spo2Seconds','hrLowLimit','hrLowSeconds','hrHighLimit','hrHighSeconds',
 'briMin','briDay','briAlarm','briTest','ldrThreshold','ldrHysteresis','volAlarm',
 'alarmRepeatSec'];
// pollSeconds is deliberately NOT in NUMS: it is a select with fixed steps
// and is loaded and saved separately (see pollSet).
const POLLS=[5,10,15];
const BOOLS=['ownAlarms','soundEnabled'];
const TXT=['wifiSsid','owletMail'];
let DEF={};

let ENP={};
function applyLang(l){
 LANG=l;
 document.querySelectorAll('[data-i18n]').forEach(e=>{
  const k=e.dataset.i18n;
  if(EN[k]===undefined)EN[k]=e.innerHTML;
  e.innerHTML=(l=='de'&&DE[k]!==undefined)?DE[k]:EN[k];
 });
 /* Placeholders in input fields. Their own store ENP, because the English
    text lives in the placeholder attribute and not in innerHTML. */
 document.querySelectorAll('[data-i18n-ph]').forEach(e=>{
  const k=e.dataset.i18nPh;
  if(ENP[k]===undefined)ENP[k]=e.placeholder;
  e.placeholder=(l=='de'&&DE[k]!==undefined)?DE[k]:ENP[k];
 });
 document.querySelectorAll('.lang button').forEach(b=>b.classList.toggle('on',b.dataset.l==l));
 document.documentElement.lang=l;
 // The alarm preview draws the word ALARM / HINWEIS itself, so it has to be
 // redrawn when the language changes.
 if(typeof pvAll=='function')pvAll();
 if(typeof pwLang=='function')pwLang();
 tick();
}
document.querySelectorAll('.lang button').forEach(b=>b.onclick=()=>{
 applyLang(b.dataset.l);
 fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},
   body:JSON.stringify({lang:b.dataset.l})}).then(()=>toast(tr('saved')));
});
// The tab lives in the address (#display). A reload then lands back on it
// instead of jumping to Status - and you can send someone a link to one
// particular tab.
const TABS=['status','display','alarms','system'];
function showTab(i){
 document.querySelectorAll('nav button').forEach(x=>x.classList.remove('on'));
 document.querySelectorAll('section').forEach(x=>x.classList.remove('on'));
 const b=document.querySelector('nav button[data-t="'+i+'"]'); if(b)b.classList.add('on');
 const s=$('s'+i); if(s)s.classList.add('on');
}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
 showTab(b.dataset.t);
 history.replaceState(null,'','#'+TABS[b.dataset.t]);
});
addEventListener('hashchange',()=>{
 const i=TABS.indexOf(location.hash.slice(1)); if(i>=0)showTab(i);
});
{ const i=TABS.indexOf(location.hash.slice(1)); if(i>0)showTab(i); }
document.querySelectorAll('.sl input').forEach(r=>{
 const o=r.nextElementSibling, u=()=>o.value=r.value; r.oninput=u; u();
});
// The warning under the minimum-brightness slider stands for as long as the
// value does - a toast shown once would be gone by the time it mattered, and
// this is a setting people come back to at night.
function briWarnUpd(){
 const e=$('briMin'), w=$('briWarn');
 if(e&&w)w.hidden=(parseInt(e.value)||1)>=5;
}
{ const e=$('briMin'); if(e){ e.addEventListener('input',briWarnUpd); briWarnUpd(); } }
function toast(x){const e=$('toast');e.innerHTML=x;e.classList.add('on');
 setTimeout(()=>e.classList.remove('on'),2300);}

// --- Live mirror ----------------------------------------------------------
const cv=$('mx'), cx=cv.getContext('2d'); let bri=1;
function paint(hex){
 // The brightness sits in the last two characters - that way it is right
 // from the very first frame instead of flashing bright for a moment.
 if(hex.length>=1538) bri=parseInt(hex.substr(1536,2),16)||1;
 // Not a linear dim: at level 1 the picture would be black.
 const CW=cv.width/32, s=$('simBri').checked?0.45+0.55*Math.pow(bri/255,0.55):1;
 cx.fillStyle='#05060a'; cx.fillRect(0,0,cv.width,cv.height);
 for(let y=0;y<8;y++)for(let x=0;x<32;x++){
  const i=(y*32+x)*6;
  const r=Math.round(parseInt(hex.substr(i,2),16)*s);
  const g=Math.round(parseInt(hex.substr(i+2,2),16)*s);
  const b=Math.round(parseInt(hex.substr(i+4,2),16)*s);
  if(r+g+b>8){cx.shadowColor='rgb('+r+','+g+','+b+')';cx.shadowBlur=CW*0.7;}else cx.shadowBlur=0;
  cx.fillStyle=r+g+b>4?'rgb('+r+','+g+','+b+')':'#0d1016';
  cx.beginPath();cx.arc(x*CW+CW/2,y*CW+CW/2,CW*0.36,0,7);cx.fill();
 }
 cx.shadowBlur=0;
}
async function frame(){ try{const h=await(await fetch('/api/frame')).text();
 if(h.length>=1536)paint(h);}catch(e){} }
// The mirror starts a little later on purpose. On load the page fetches
// settings, status and frame at the same time; the small web server on the
// ESP32 only serves one connection at a time, and the settings request was
// the one that got lost - leaving the interface sitting there with black
// colour swatches.
setTimeout(()=>{ frame(); setInterval(frame,200); },900);

// --- Status ---------------------------------------------------------------
async function tick(){
 let d; try{d=await(await fetch('/api/state')).json();}catch(e){return;}
 const de=LANG=='de';
 $('pScreen').textContent=de?d.screenNameDe:d.screenName;
 $('pScreen').className='pill '+(d.alarm?'bad':(d.fresh?'ok':'warn'));
 $('vHr').textContent=d.fresh&&d.hr>0?d.hr:'--';
 $('vOx').textContent=d.fresh&&d.ox>0?d.ox:'--';
 $('vBat').textContent=d.bat;
 $('dScreen').textContent=de?d.screenNameDe:d.screenName;
 $('dWhy').textContent=de?d.whyDe:d.why;
 $('dBri').textContent=d.bri+' / 255';
 $('dLdr').textContent=d.ldr+(d.ambient?tr('bright'):tr('dark'));
 $('ldrNow').textContent=d.ldr;
 $('dSleep').textContent=(de?d.sleepNameDe:d.sleepName)+' ('+d.ss+')';
 $('dChg').textContent=d.chg?tr('yes')+' ('+d.chg+')':tr('no');
 $('dOff').textContent=d.sockOff?tr('yes'):tr('no');
 $('dBso').textContent=d.bso?tr('yes'):tr('no');
 $('dHw').textContent=d.hw||'-';
 $('dWifi').textContent=d.ssid+' ('+d.rssi+' dBm)  '+d.ip;
 $('dLogin').textContent=d.loggedIn?tr('on'):tr('off');
 $('dAge').textContent=d.age+tr('sAgo');
 $('dCnt').textContent=d.polls+' / '+d.fails;
 $('dTok').textContent=Math.round(d.tok/60)+tr('min');
 $('dHeap').textContent=d.heap+' kB / '+d.block+' kB';
 $('dUp').textContent=Math.round(d.up/60)+tr('min');
 $('errRow').innerHTML=d.err?'<div class=row><span>'+tr('lastErr')+
   '</span><span style=color:var(--warn)>'+esc(d.err)+'</span></div>':'';
 $('alarmHere').innerHTML=d.alarmText?'<div class=alarmbox><b>'+
   (d.alarm?tr('alarm'):tr('notice'))+'</b><br>'+esc(d.alarmText)+
   (d.alarm?'<br><button class="b g" style=margin-top:8px onclick=hush()>'+tr('hush')+'</button>':'')+
   '</div>':'';
}
setInterval(tick,1500);
</script>)HTML";

static const char P_JS3[] PROGMEM = R"HTML(<script>
const hx=n=>'#'+('000000'+(n>>>0).toString(16)).slice(-6);
const t=m=>{ if(m==0)MIRON=false; return fetch('/api/test?m='+m+'&s=25'); };

/* ---------------------------------------------------------------------------
   Mirroring onto the clock

   The canvas above is a good likeness, but no screen can say how a muted
   colour reads on the matrix in a dark room - and that is exactly what these
   colours are chosen for. So every change is pushed to the clock as well and
   shown there for ten seconds, without being saved. The requests are
   debounced: dragging a colour picker fires a great many input events and the
   web server on the ESP32 answers one connection at a time.
*/
// variant -> which test screen belongs to it (same numbers as the buttons)
const PVSCR={vitals:4,sleep0:4,sleep1:4,sleep2:4,sleep3:4,
 bat0:5,bat1:5,bat2:5,bat3:5,wait:6,offline:7,alarm:8,info:9};
// MIRREADY stays false until the fields have been filled from the device:
// loading the config fires an input event on every slider, and opening the
// page must not push the clock into a preview.
let MIRSCR=4, MIRON=false, MIRT=null, MIRREADY=false;
function palNow(){
 const p={}; COLS.forEach(k=>{const e=$('c_'+k);
  if(e)p[k]=parseInt(e.value.slice(1),16);}); return p;
}
function mirror(scr){
 if(!MIRREADY)return;
 if(scr!==undefined)MIRSCR=scr;
 clearTimeout(MIRT);
 MIRT=setTimeout(()=>{
  MIRON=true;
  const b={pal:palNow(),m:MIRSCR};
  const e=$('briTest'); if(e)b.briTest=parseInt(e.value)||40;
  fetch('/api/preview',{method:'POST',headers:{'Content-Type':'application/json'},
   body:JSON.stringify(b)}).catch(()=>{});
 },200);
}
// Leaving the page counts as cancelling: better a clock that is back to
// normal than one left showing a preview nobody is watching.
function mirrorStop(){
 clearTimeout(MIRT);
 if(!MIRON)return;
 MIRON=false;
 fetch('/api/preview/stop',{method:'POST'}).catch(()=>{});
}
addEventListener('pagehide',mirrorStop);
const snd=w=>fetch('/api/sound?w='+w);
const hush=()=>fetch('/api/hush',{method:'POST'});

function addReset(e,k){
 const row=e.parentElement;
 if(row.querySelector('.rs'))return;
 const b=document.createElement('button');
 b.className='rs'; b.type='button'; b.title='reset';
 b.textContent='↺';
 // Setting value by hand fires NO input event - otherwise the preview would
 // stay on the old colour.
 b.onclick=()=>{ if(DEF[k]!==undefined){ e.value=hx(DEF[k]); pvAll();
  mirror(PVSCR[e.dataset.pv]); } };
 row.appendChild(b);
}
function resetAll(){ COLS.forEach(k=>{const e=$('c_'+k);
 if(e&&DEF[k]!==undefined)e.value=hx(DEF[k]);}); pvAll(); mirror();
 toast(tr('colsReset')); }

/* ---------------------------------------------------------------------------
   Colour preview

   Draws the same screens as display.cpp, but in the browser and with the
   colours currently sitting in the fields. That shows the effect at once,
   without saving and without switching the clock beside the cot.

   The glyphs arrive as a hex string from the device (dispFontHex), so the
   type cannot drift. The LAYOUT of the screens, however, exists twice: here
   and in display.cpp. Move screenVitals & co. over there and the twins here
   have to follow - each one is marked as such below.
--------------------------------------------------------------------------- */
let FONT=[];
function fontLoad(hex){
 FONT=[];
 for(let i=0;i+6<=hex.length;i+=6)
  FONT.push([parseInt(hex.substr(i,2),16),parseInt(hex.substr(i+2,2),16),
             parseInt(hex.substr(i+4,2),16)]);
}
const cval=id=>{const e=$(id); return e?parseInt(e.value.slice(1),16):0;};
const nbuf=()=>new Array(256).fill(0);
function ppx(b,x,y,c){ if(x>=0&&x<32&&y>=0&&y<8) b[y*32+x]=c; }
function prect(b,x,y,w,h,c){ for(let j=0;j<h;j++)for(let i=0;i<w;i++)ppx(b,x+i,y+j,c); }
function pchar(b,x,ch,c){
 if(x>32||x<-4) return x+4;
 let k=ch.toUpperCase().charCodeAt(0);
 if(k<32||k>90)k=32;
 const g=FONT[k-32]||[0,0,0];
 for(let col=0;col<3;col++)for(let row=0;row<5;row++)
  if(g[col]&(1<<row)) ppx(b,x+col,1+row,c);
 return x+4;
}
function ptext(b,x,s,c){ for(const ch of s) x=pchar(b,x,ch,c); }
const pwid=s=>s.length*4-1;
function pnum(b,rx,v,c){ const s=String(v); ptext(b,rx-pwid(s)+1,s,c); }
function pheart(b,c){
 ppx(b,1,1,c); ppx(b,3,1,c);
 for(let x=0;x<=4;x++){ ppx(b,x,2,c); ppx(b,x,3,c); }
 for(let x=1;x<=3;x++) ppx(b,x,4,c);
 ppx(b,2,5,c);
}
// Twin of screenVitals(). ss: 1 awake, 2 light, 3 deep, otherwise unknown.
function pvVitals(ss){
 const b=nbuf();
 pheart(b,cval('c_heart'));
 pnum(b,17,120,cval('c_numbers'));
 ppx(b,19,3,cval('c_sep'));
 pnum(b,31,98,cval('c_numbers'));
 let x=15,w=2,c=cval('c_sleepUnk');
 if(ss==1){x=7;w=18;c=cval('c_awake');}
 else if(ss==2){x=11;w=10;c=cval('c_lightSleep');}
 else if(ss==3){x=14;w=4;c=cval('c_deepSleep');}
 prect(b,x,7,w,1,c);
 return b;
}
// Twin of screenBattery().
function pvBattery(pct,charging){
 const b=nbuf(), fr=cval('c_batFrame');
 prect(b,0,1,10,1,fr); prect(b,0,5,10,1,fr);
 prect(b,0,2,1,3,fr);  prect(b,9,2,1,3,fr); prect(b,10,2,1,3,fr);
 const w=Math.min(8,Math.max(1,Math.floor(pct*8/100)));
 const c=charging?cval('c_batCharge')
        :(pct<20?cval('c_batLow'):(pct<40?cval('c_batMid'):cval('c_batOk')));
 prect(b,1,2,w,3,c);
 pnum(b,28,pct,cval('c_batText'));
 ptext(b,29,'%',cval('c_batFrame'));
 return b;
}
// Twin of screenWaiting().
function pvWaiting(){
 const b=nbuf();
 pheart(b,cval('c_heartWait'));
 ptext(b,10,'--',cval('c_dashes'));
 ppx(b,19,3,cval('c_sep'));
 ptext(b,25,'--',cval('c_dashes'));
 prect(b,15,7,2,1,cval('c_sleepUnk'));
 return b;
}
// Twin of screenOffline().
function pvOffline(){ const b=nbuf(); ptext(b,2,'OFFLINE',cval('c_offline')); return b; }
// Twin of screenAlarm(). On the clock the text scrolls and the three corner
// pixels blink; here both stand still - this is about the colour.
function pvAlarmScr(crit){
 const b=nbuf(), c=crit?cval('c_alarm'):cval('c_info');
 ptext(b,1,crit?tr('alarm'):tr('notice'),c);
 if(crit){ ppx(b,0,0,c); ppx(b,15,0,c); ppx(b,31,0,c); }
 return b;
}

// variant -> [canvas, painter]. Every colour field carries the variant it
// belongs to as data-pv; touching it switches the preview to that case.
const PV={
 vitals:['pvVit',()=>pvVitals(1)],
 sleep0:['pvSleep',()=>pvVitals(0)], sleep1:['pvSleep',()=>pvVitals(1)],
 sleep2:['pvSleep',()=>pvVitals(2)], sleep3:['pvSleep',()=>pvVitals(3)],
 bat0:['pvBat',()=>pvBattery(74,false)], bat1:['pvBat',()=>pvBattery(74,true)],
 bat2:['pvBat',()=>pvBattery(35,false)], bat3:['pvBat',()=>pvBattery(12,false)],
 wait:['pvWait',pvWaiting], offline:['pvWait',pvOffline],
 alarm:['pvAlarm',()=>pvAlarmScr(true)], info:['pvAlarm',()=>pvAlarmScr(false)]
};
let PVNOW={pvVit:'vitals',pvSleep:'sleep1',pvBat:'bat0',pvWait:'wait',pvAlarm:'alarm'};

function pvPaint(id){
 const cv=$(id); if(!cv||!FONT.length) return;
 const f=PV[PVNOW[id]]; if(!f) return;
 const b=f[1](), cx=cv.getContext('2d'), CW=cv.width/32;
 cx.fillStyle='#05060a'; cx.fillRect(0,0,cv.width,cv.height);
 for(let y=0;y<8;y++)for(let x=0;x<32;x++){
  const v=b[y*32+x], r=(v>>16)&255, g=(v>>8)&255, bl=v&255;
  if(r+g+bl>8){ cx.shadowColor='rgb('+r+','+g+','+bl+')'; cx.shadowBlur=CW*0.7; }
  else cx.shadowBlur=0;
  cx.fillStyle=(r+g+bl>4)?'rgb('+r+','+g+','+bl+')':'#0d1016';
  cx.beginPath(); cx.arc(x*CW+CW/2,y*CW+CW/2,CW*0.36,0,7); cx.fill();
 }
 cx.shadowBlur=0;
}
function pvAll(){ Object.keys(PVNOW).forEach(pvPaint); }
function pvWire(){
 document.querySelectorAll('[data-pv]').forEach(e=>{
  const k=e.dataset.pv, id=PV[k]&&PV[k][0];
  if(!id) return;
  const show=()=>{ PVNOW[id]=k; pvAll(); };
  // Focus only switches the canvas. Taking over the clock because somebody
  // tabbed through the fields would be a nasty surprise next to a cot;
  // an actual change is a different matter.
  e.addEventListener('input',()=>{ show(); mirror(PVSCR[k]); });
  e.addEventListener('focus',show);
 });
 // The preview brightness reaches the clock the same way. addEventListener,
 // not oninput: the slider readout already sits on that property.
 const b=$('briTest');
 if(b)b.addEventListener('input',()=>mirror());
}

// A switch that governs a whole block: the block dims and the fields inside
// it cannot be used while it is off.
function gate(cbId,boxId){
 const cb=$(cbId), box=$(boxId);
 if(!cb||!box) return;
 const upd=()=>{
  box.classList.toggle('off',!cb.checked);
  box.querySelectorAll('input,button,select').forEach(e=>e.disabled=!cb.checked);
 };
 cb.addEventListener('change',upd);
 upd();
}

// Poll interval: fixed steps. An older, freely typed value snaps to the
// nearest step rather than leaving the field blank.
function pollSet(v){
 const n=POLLS.reduce((a,b)=>Math.abs(b-v)<Math.abs(a-v)?b:a,POLLS[0]);
 const e=$('pollSeconds'); if(e)e.value=n;
}

async function scan(){
 const d=$('nets'); d.innerHTML='<p class=note>'+tr('searching')+'</p>';
 try{
  const l=await(await fetch('/api/scan')).json();
  if(!l.length){d.innerHTML='<p class=note>'+tr('noNets')+'</p>';return;}
  d.innerHTML=l.map(n=>'<div class=sw style=cursor:pointer onclick="pick(this)" data-s="'+
   esc(n.s)+'"><span>'+esc(n.s)+(n.e?' &#128274;':'')+'</span><small style=color:var(--mut)>'+
   n.r+' dBm</small></div>').join('');
 }catch(e){ d.innerHTML='<p class=note>'+tr('scanFail')+'</p>'; }
}
function pick(el){ $('wifiSsid').value=el.dataset.s; $('wifiPass').focus(); }

async function loadCfg(tries){
 if(tries===undefined)tries=4;
 let c;
 try{ c=await(await fetch('/api/config')).json(); }
 catch(e){
  // A lost request must not leave the interface sitting there with empty
  // fields: wait a moment and ask again.
  if(tries>0){ setTimeout(()=>loadCfg(tries-1),700); return; }
  toast(tr('cfgFail')); return;
 }
 DEF=c.palDef||{};
 COLS.forEach(k=>{const e=$('c_'+k); if(e){e.value=hx(c.pal[k]); addReset(e,k);}});
 NUMS.forEach(k=>{const e=$(k); if(e){e.value=c[k];
  if(e.type=='range')e.dispatchEvent(new Event('input'));}});
 BOOLS.forEach(k=>{const e=$(k); if(e)e.checked=!!c[k];});
 TXT.forEach(k=>{const e=$(k); if(e)e.value=c[k]||'';});
 $('europe').value=c.europe?1:0;
 pollSet(parseInt(c.pollSeconds)||10);
 fontLoad(c.font||'');
 pvWire(); pvAll();
 gate('ownAlarms','gArm'); gate('soundEnabled','gSound');
 applyLang(c.lang=='de'?'de':'en');
 MIRREADY=true;   // from here on a change is a change the user made
}
async function save(){
 // Saving ends the preview. Drop a debounced push still in flight as well,
 // otherwise it would fire straight after the save and put the clock back
 // into a test screen it has just been taken out of.
 clearTimeout(MIRT); MIRON=false;
 const b={pal:{},lang:LANG};
 COLS.forEach(k=>{const e=$('c_'+k); if(e)b.pal[k]=parseInt(e.value.slice(1),16);});
 NUMS.forEach(k=>{const e=$(k); if(e)b[k]=parseInt(e.value)||0;});
 BOOLS.forEach(k=>{const e=$(k); if(e)b[k]=e.checked;});
 TXT.forEach(k=>{const e=$(k); if(e)b[k]=e.value;});
 b.europe=$('europe').value=='1';
 b.pollSeconds=parseInt($('pollSeconds').value)||10;
 // An empty password field means: leave it unchanged.
 ['wifiPass','owletPass','webPass'].forEach(k=>{if($(k)&&$(k).value)b[k]=$(k).value;});
 const r=await(await fetch('/api/config',{method:'POST',
   headers:{'Content-Type':'application/json'},body:JSON.stringify(b)})).json();
 ['wifiPass','owletPass','webPass'].forEach(k=>{if($(k))$(k).value='';});
 toast(r.reboot?tr('savedReboot'):tr('saved'));
}
async function ota(){
 const f=$('fw').files[0]; if(!f){alert(tr('pickBin'));return;}
 if(!confirm(tr('confOta').replace('%1',f.name).replace('%2',Math.round(f.size/1024))))return;
 const p=$('otaProg'); p.textContent=tr('uploading');
 const fd=new FormData(); fd.append('f',f,f.name);
 try{ const r=await fetch('/api/ota',{method:'POST',body:fd}); const x=await r.text();
  p.textContent = r.ok ? tr('otaOk') : (tr('otaFail')+x);
 }catch(e){ p.textContent=tr('otaLost'); }
}
async function factory(){
 if(!confirm(tr('confFact')))return;
 try{ await fetch('/api/factory',{method:'POST'}); }catch(e){}
 const de=LANG=='de';
 document.body.innerHTML='<div style="max-width:520px;margin:70px auto;padding:24px;'+
  'font:16px/1.6 system-ui;color:#e8ebf0"><h2 style=margin-top:0>'+
  (de?'Zurückgesetzt':'Reset done')+'</h2><p>'+
  (de?'Das Gerät startet neu und öffnet den offenen WLAN-Hotspot <b>owlanzi</b>.'
     :'The device restarts and opens the open Wi-Fi hotspot <b>owlanzi</b>.')+'</p>'+
  '<p style=color:#8d97a6>'+(de?'Mit diesem Netz verbinden - die Einrichtungsseite geht von '+
  'selbst auf. Sonst <b>192.168.4.1</b> aufrufen.':'Join that network - the setup page opens '+
  'by itself. Otherwise open <b>192.168.4.1</b>.')+'</p></div>';
}
loadCfg();
</script>)HTML";

// ---------------------------------------------------------------------------
//  Endpoints
// ---------------------------------------------------------------------------
static bool guard() {
  if (!strlen(gCfg.webPass)) return true;
  if (srv.authenticate("owlanzi", gCfg.webPass)) return true;
  srv.requestAuthentication();
  return false;
}

static void jesc(char *d, size_t n, const char *s) {
  size_t o = 0;
  for (; *s && o + 2 < n; s++) {
    if (*s == '"' || *s == '\\') { d[o++] = '\\'; d[o++] = *s; }
    else if ((uint8_t)*s >= 32)  { d[o++] = *s; }
  }
  d[o] = 0;
}

// Send both languages: the interface can then switch without reloading, and
// the firmware does not have to know what the browser currently has set.
static const char *scrEn(ScreenId s) {
  switch (s) { case SCR_ALARM: return "Alarm"; case SCR_VITALS: return "Vitals";
    case SCR_BATTERY: return "Battery"; case SCR_WAITING: return "Waiting for values";
    case SCR_OFFLINE: return "Offline"; case SCR_SETUP: return "Setup";
    case SCR_MESSAGE: return "Message"; default: return "Preview"; }
}
static const char *scrDe(ScreenId s) {
  switch (s) { case SCR_ALARM: return "Alarm"; case SCR_VITALS: return "Vitalwerte";
    case SCR_BATTERY: return "Akku"; case SCR_WAITING: return "Warten auf Werte";
    case SCR_OFFLINE: return "Offline"; case SCR_SETUP: return "Einrichtung";
    case SCR_MESSAGE: return "Meldung"; default: return "Vorschau"; }
}
static const char *sleepEn(int ss) {
  switch (ss) { case 1: case 8: return "awake"; case 2: case 9: return "light sleep";
    case 3: case 10: return "deep sleep"; default: return "unknown"; }
}
static const char *sleepDe(int ss) {
  switch (ss) { case 1: case 8: return "wach"; case 2: case 9: return "leichter Schlaf";
    case 3: case 10: return "Tiefschlaf"; default: return "unbekannt"; }
}
// Why this screen and not another? So nothing has to be guessed in the
// browser.
static const char *whyEn() {
  if (gSt.testMode)  return "preview running";
  if (gSt.apMode)    return "setup, no Wi-Fi yet";
  if (gSt.screen == SCR_MESSAGE) return "showing a message";
  if (gAlarmText.length() && !(gAlarmCritical && gSt.silenced)) return "alarm active";
  if (!gSt.cloudOk || millis() - gSt.lastOkAt > 20000UL) return "no data for over 20 s";
  if (gSt.v.charging != 0) return "sock is charging";
  if (gSt.v.sockOff)       return "sock is off the foot";
  if (!vitalsFresh())      return "readings are not current - never showing stale values";
  return "fresh readings";
}
static const char *whyDe() {
  if (gSt.testMode)  return "Vorschau läuft";
  if (gSt.apMode)    return "Einrichtung, noch kein WLAN";
  if (gSt.screen == SCR_MESSAGE) return "zeigt eine Meldung";
  if (gAlarmText.length() && !(gAlarmCritical && gSt.silenced)) return "Alarm liegt an";
  if (!gSt.cloudOk || millis() - gSt.lastOkAt > 20000UL) return "seit über 20 s keine Daten";
  if (gSt.v.charging != 0) return "Socke lädt";
  if (gSt.v.sockOff)       return "Socke ist abgelegt";
  if (!vitalsFresh())      return "Werte sind nicht aktuell - alte werden nie gezeigt";
  return "frische Messwerte";
}

static void handleState() {
  if (!guard()) return;
  static char buf[1400], e1[64], e2[120], e3[220];
  stateLock();
  Vitals v = gSt.v;
  String al = gAlarmText;
  bool crit = gAlarmCritical;
  uint32_t age = (millis() - gSt.lastOkAt) / 1000;
  stateUnlock();

  jesc(e1, sizeof(e1), WiFi.SSID().c_str());
  jesc(e2, sizeof(e2), gSt.lastError);
  jesc(e3, sizeof(e3), al.c_str());
  bool fresh = vitalsFresh();

  snprintf(buf, sizeof(buf),
    "{\"screen\":%u,\"screenName\":\"%s\",\"screenNameDe\":\"%s\","
    "\"why\":\"%s\",\"whyDe\":\"%s\",\"bri\":%u,"
    "\"ldr\":%d,\"ambient\":%s,\"fresh\":%s,\"alarm\":%s,\"alarmText\":\"%s\","
    "\"hr\":%d,\"ox\":%d,\"bat\":%d,\"ss\":%d,\"sleepName\":\"%s\",\"sleepNameDe\":\"%s\","
    "\"chg\":%d,\"sockOff\":%s,\"bso\":%s,\"hw\":\"%s\","
    "\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"%s\",\"loggedIn\":%s,"
    "\"age\":%u,\"polls\":%u,\"fails\":%u,\"tok\":%u,"
    "\"heap\":%u,\"block\":%u,\"up\":%u,\"ap\":%s,\"err\":\"%s\"}",
    (unsigned)gSt.screen, scrEn(gSt.screen), scrDe(gSt.screen), whyEn(), whyDe(),
    gSt.brightness, gSt.ldrRaw, gSt.ambientBright ? "true" : "false",
    fresh ? "true" : "false", (crit && !gSt.silenced) ? "true" : "false", e3,
    (int)lroundf(v.heart), (int)lroundf(v.oxygen), (int)lroundf(v.battery),
    v.sleepSt, sleepEn(v.sleepSt), sleepDe(v.sleepSt), v.charging,
    v.sockOff ? "true" : "false", v.baseOn ? "true" : "false", v.hardware,
    e1, (int)WiFi.RSSI(), WiFi.localIP().toString().c_str(),
    gSt.loggedIn ? "true" : "false",
    (unsigned)age, (unsigned)gSt.pollCount, (unsigned)gSt.failCount,
    (unsigned)owletTokenSecondsLeft(),
    (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024),
    (unsigned)(millis() / 1000), gSt.apMode ? "true" : "false", e2);
  srv.send(200, "application/json", buf);
}

static void handleFrame() { if (!guard()) return; srv.send(200, "text/plain", dispFrameHex()); }

/* ---------------------------------------------------------------------------
   Live preview on the clock

   While somebody drags a colour in the Display tab, the clock itself should
   show the change at once - the mirror in the browser cannot say how a colour
   really looks on the matrix in a dark room, which is the whole point of
   picking muted colours. So the palette in gCfg is overwritten in RAM only,
   the saved one is parked here, and everything falls back after ten seconds,
   on Save, or on Back to normal. A browser tab left open beside the cot can
   therefore never leave the clock in a colour that was never saved.
*/
static bool    gPrevOn      = false;
static Palette gPrevPal;
static int     gPrevBriTest = 0;

static void previewBegin() {
  if (gPrevOn) return;
  gPrevPal = gCfg.pal; gPrevBriTest = gCfg.briTest;
  gPrevOn = true;
}
static void previewEnd(bool restore) {
  if (!gPrevOn) return;
  gPrevOn = false;
  if (restore) { gCfg.pal = gPrevPal; gCfg.briTest = gPrevBriTest; }
}
// dispTick() clears testMode when the preview time is up - that is the moment
// the saved colours have to come back.
static void previewTick() { if (gPrevOn && !gSt.testMode) previewEnd(true); }

// Colours out of a JSON object, used by both the save and the preview route.
// A missing key keeps the current value, so a partial body is harmless.
static void palFromJson(JsonObject p) {
  if (p.isNull()) return;
  Palette &q = gCfg.pal;
  auto C=[&](const char*k,uint32_t&dst){ if(!p[k].isNull()) dst = p[k]|dst; };
  C("heart",q.heart); C("numbers",q.numbers); C("sep",q.sep);
  C("heartWait",q.heartWait); C("dashes",q.dashes); C("awake",q.awake);
  C("lightSleep",q.lightSleep); C("deepSleep",q.deepSleep); C("sleepUnk",q.sleepUnk);
  C("batFrame",q.batFrame); C("batOk",q.batOk); C("batCharge",q.batCharge);
  C("batMid",q.batMid); C("batLow",q.batLow); C("batText",q.batText);
  C("alarm",q.alarm); C("info",q.info); C("offline",q.offline);
}

static void handleConfigGet() {
  if (!guard()) return;
  JsonDocument d;
  d["europe"]=gCfg.europe; d["ownAlarms"]=gCfg.ownAlarms;
  d["soundEnabled"]=gCfg.soundEnabled;
  d["wifiSsid"]=gCfg.wifiSsid; d["owletMail"]=gCfg.owletMail; d["lang"]=gCfg.lang;
  d["spo2Limit"]=gCfg.spo2Limit;   d["spo2Seconds"]=gCfg.spo2Seconds;
  d["hrLowLimit"]=gCfg.hrLowLimit; d["hrLowSeconds"]=gCfg.hrLowSeconds;
  d["hrHighLimit"]=gCfg.hrHighLimit; d["hrHighSeconds"]=gCfg.hrHighSeconds;
  d["briMin"]=gCfg.briMin; d["briDay"]=gCfg.briDay;
  // While a preview is running the RAM values are temporary. Report what is
  // actually stored, so a second tab does not mistake them for saved ones.
  d["briAlarm"]=gCfg.briAlarm; d["briTest"]=gPrevOn ? gPrevBriTest : gCfg.briTest;
  d["ldrThreshold"]=gCfg.ldrThreshold; d["ldrHysteresis"]=gCfg.ldrHysteresis;
  d["volAlarm"]=gCfg.volAlarm; d["alarmRepeatSec"]=gCfg.alarmRepeatSec;
  d["pollSeconds"]=gCfg.pollSeconds;
  // The font travels along: the colour preview in the browser redraws the
  // screens itself and should use the same glyphs as the matrix.
  d["font"]=dispFontHex();
  const Palette &q = gPrevOn ? gPrevPal : gCfg.pal; Palette dflt;
  JsonObject p = d["pal"].to<JsonObject>();
  JsonObject e = d["palDef"].to<JsonObject>();
  #define PC(k) p[#k]=q.k; e[#k]=dflt.k;
  PC(heart) PC(numbers) PC(sep) PC(heartWait) PC(dashes) PC(awake) PC(lightSleep)
  PC(deepSleep) PC(sleepUnk) PC(batFrame) PC(batOk) PC(batCharge) PC(batMid)
  PC(batLow) PC(batText) PC(alarm) PC(info) PC(offline)
  #undef PC
  String out; serializeJson(d, out);
  srv.send(200, "application/json", out);
}

static void handleConfigPost() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, srv.arg("plain"))) { srv.send(400, "application/json", "{\"err\":1}"); return; }

  // A save ends any running preview, and it restores first: a partial body -
  // the language buttons post nothing but {lang} - would otherwise write the
  // temporary preview colours into the NVS as if they had been chosen.
  if (gPrevOn) { previewEnd(true); dispTest(TEST_OFF, 0); }

  char oSsid[33]; strlcpy(oSsid, gCfg.wifiSsid, sizeof(oSsid));
  char oPass[65]; strlcpy(oPass, gCfg.wifiPass, sizeof(oPass));
  char oMail[65]; strlcpy(oMail, gCfg.owletMail, sizeof(oMail));
  char oOw[65];   strlcpy(oOw, gCfg.owletPass, sizeof(oOw));

  auto S=[&](const char*k,char*dst,size_t n){ if(d[k].is<const char*>()) strlcpy(dst, d[k]|"", n); };
  auto I=[&](const char*k,int&dst){ if(!d[k].isNull()) dst = d[k]|dst; };
  auto B=[&](const char*k,bool&dst){ if(!d[k].isNull()) dst = d[k]|dst; };
  S("wifiSsid",gCfg.wifiSsid,sizeof(gCfg.wifiSsid));
  S("wifiPass",gCfg.wifiPass,sizeof(gCfg.wifiPass));
  S("owletMail",gCfg.owletMail,sizeof(gCfg.owletMail));
  S("owletPass",gCfg.owletPass,sizeof(gCfg.owletPass));
  S("webPass",gCfg.webPass,sizeof(gCfg.webPass));
  S("lang",gCfg.lang,sizeof(gCfg.lang));
  B("europe",gCfg.europe); B("ownAlarms",gCfg.ownAlarms);
  B("soundEnabled",gCfg.soundEnabled);
  I("spo2Limit",gCfg.spo2Limit); I("spo2Seconds",gCfg.spo2Seconds);
  I("hrLowLimit",gCfg.hrLowLimit); I("hrLowSeconds",gCfg.hrLowSeconds);
  I("hrHighLimit",gCfg.hrHighLimit); I("hrHighSeconds",gCfg.hrHighSeconds);
  I("briMin",gCfg.briMin); I("briDay",gCfg.briDay);
  I("briAlarm",gCfg.briAlarm); I("briTest",gCfg.briTest);
  I("ldrThreshold",gCfg.ldrThreshold); I("ldrHysteresis",gCfg.ldrHysteresis);
  I("volAlarm",gCfg.volAlarm); I("alarmRepeatSec",gCfg.alarmRepeatSec);
  I("pollSeconds",gCfg.pollSeconds);
  // The interface only offers 5, 10 and 15 s. This guard is for anything
  // that does not come through the interface: a 0 would call the Owlet
  // cloud in a loop.
  if (gCfg.pollSeconds < 3)   gCfg.pollSeconds = 3;
  if (gCfg.pollSeconds > 120) gCfg.pollSeconds = 120;
  palFromJson(d["pal"]);
  cfgSave();
  alarmRecompute();

  bool wifiChanged  = strcmp(oSsid, gCfg.wifiSsid) || strcmp(oPass, gCfg.wifiPass);
  bool owletChanged = strcmp(oMail, gCfg.owletMail) || strcmp(oOw, gCfg.owletPass);
  if (owletChanged) { gSt.loggedIn = false; gSt.dsn[0] = 0; }

  srv.send(200, "application/json", wifiChanged ? "{\"ok\":1,\"reboot\":1}" : "{\"ok\":1}");
  if (wifiChanged) { delay(700); ESP.restart(); }
}

static void handleScan() {
  if (!guard()) return;
  int n = WiFi.scanNetworks();
  String out = "[";
  for (int i = 0; i < n && i < 24; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    s.replace("\\", ""); s.replace("\"", "'");
    if (out.length() > 1) out += ",";
    out += "{\"s\":\"" + s + "\",\"r\":" + String(WiFi.RSSI(i)) +
           ",\"e\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1) + "}";
  }
  WiFi.scanDelete();
  srv.send(200, "application/json", out + "]");
}

// Firmware update. Written into the second program partition; the switch
// only happens once the image is complete and valid.
static void handleOtaUpload() {
  HTTPUpload &u = srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    if (strlen(gCfg.webPass) && !srv.authenticate("owlanzi", gCfg.webPass)) return;
    Serial.printf("OTA starting: %s\n", u.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.isRunning() && Update.write(u.buf, u.currentSize) != u.currentSize)
      Update.printError(Serial);
  } else if (u.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("OTA done, %u bytes\n", (unsigned)u.totalSize);
    else Update.printError(Serial);
  }
}

// The setup page. While the device sits in the hotspot it is the start page;
// under /setup it can also be viewed during normal operation - handy for
// checking it without resetting everything.
static void sendSetup() {
  srv.sendContent_P(SETUP_PAGE);
  srv.sendContent_P(CSS);
  srv.sendContent_P(PW_JS);
  srv.sendContent_P(PSTR("<div class=w><div style=\"padding:20px 0 6px\" class=title><h1>"));
  srv.sendContent_P(LOGO);
  srv.sendContent_P(SETUP_BODY);
}

static void handleRoot() {
  if (!guard()) return;
  srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  srv.send(200, "text/html; charset=utf-8", "");
  if (gSt.apMode) {
    // Hotspot mode shows setup only: somebody who has just unboxed the clock
    // should not have to hunt for the Wi-Fi field between colour pickers and
    // alarm limits.
    sendSetup();
  } else {
    srv.sendContent_P(P_HEAD);
    srv.sendContent_P(CSS);
    srv.sendContent_P(PW_JS);
    srv.sendContent_P(P_A);
    srv.sendContent_P(LOGO);
    srv.sendContent_P(P_B);
    srv.sendContent_P(P_C);
    srv.sendContent_P(P_D);
    srv.sendContent_P(P_JS1);
    srv.sendContent_P(P_JS2);
    srv.sendContent_P(P_JS3);
  }
  srv.sendContent("");
}

void webBegin() {
  srv.on("/", handleRoot);
  srv.on("/setup", []() {
    if (!guard()) return;
    srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
    srv.send(200, "text/html; charset=utf-8", "");
    sendSetup();
    srv.sendContent("");
  });
  // No guard() on the icon: a tab icon gives nothing away, and the browser
  // asks for it before the login has happened.
  srv.on("/favicon.svg", []() {
    srv.sendHeader("Cache-Control", "max-age=604800");
    srv.send_P(200, "image/svg+xml", FAVICON);
  });
  srv.on("/api/state", handleState);
  srv.on("/api/frame", handleFrame);
  srv.on("/api/scan", handleScan);
  srv.on("/api/config", HTTP_GET, handleConfigGet);
  srv.on("/api/config", HTTP_POST, handleConfigPost);
  srv.on("/api/ota", HTTP_POST, []() {
    bool ok = !Update.hasError();
    srv.sendHeader("Connection", "close");
    srv.send(ok ? 200 : 500, "text/plain", ok ? "ok" : "update failed");
    if (ok) { delay(600); ESP.restart(); }
  }, handleOtaUpload);
  srv.on("/api/test", []() {
    if (!guard()) return;
    uint8_t m = (uint8_t)srv.arg("m").toInt();
    // Back to normal also drops a running colour preview - that is the
    // "cancel" for everything that was tried out but not saved.
    if (m == TEST_OFF) previewEnd(true);
    dispTest(m, srv.hasArg("s") ? srv.arg("s").toInt() : 20);
    srv.send(200, "application/json", "{\"ok\":1}");
  });
  /*
   * Try colours out on the matrix without saving them. The body carries the
   * palette as it currently stands in the fields, optionally a preview
   * brightness, and m = which screen to show. Ten seconds, then the saved
   * state comes back on its own; every further drag renews the ten seconds.
   */
  srv.on("/api/preview", HTTP_POST, []() {
    if (!guard()) return;
    JsonDocument d;
    if (deserializeJson(d, srv.arg("plain"))) { srv.send(400, "application/json", "{\"err\":1}"); return; }
    previewBegin();
    palFromJson(d["pal"]);
    if (!d["briTest"].isNull())
      gCfg.briTest = constrain((int)(d["briTest"] | gCfg.briTest), 1, 255);
    dispTest((uint8_t)(d["m"] | (int)TEST_VITALS), 10);
    srv.send(200, "application/json", "{\"ok\":1}");
  });
  srv.on("/api/preview/stop", HTTP_POST, []() {
    if (!guard()) return;
    previewEnd(true);
    dispTest(TEST_OFF, 0);
    srv.send(200, "application/json", "{\"ok\":1}");
  });
  srv.on("/api/sound", []() {
    if (!guard()) return;
    if (srv.arg("w") == "alarm") soundAlarm(); else soundBeep(1760, 150);
    srv.send(200, "application/json", "{\"ok\":1}");
  });
  // Acknowledge the tone: the alarm stays on screen, only the repeating
  // sound goes quiet. The acknowledgement lapses once the alarm is over.
  srv.on("/api/hush", HTTP_POST, []() {
    if (!guard()) return;
    gSt.silenced = true; soundStop();
    srv.send(200, "application/json", "{\"ok\":1}");
  });
  srv.on("/api/factory", HTTP_POST, []() {
    if (!guard()) return;
    cfgFactoryReset();
    srv.send(200, "application/json", "{\"ok\":1}");
    delay(600); ESP.restart();
  });
  srv.onNotFound([]() { srv.sendHeader("Location", "/"); srv.send(303, "text/plain", ""); });
  srv.begin();
  Serial.printf("web interface: http://%s/\n",
                gSt.apMode ? WiFi.softAPIP().toString().c_str()
                           : WiFi.localIP().toString().c_str());
}

void webTick() { srv.handleClient(); previewTick(); }
