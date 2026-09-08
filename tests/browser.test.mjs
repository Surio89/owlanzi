import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const source=readFileSync(new URL('../.pio/tests/main.js',import.meta.url),'utf8');
test('save cannot overwrite device settings before configuration has loaded',async()=>{
  let message='',calls=0;
  const ctx={MIRREADY:false,tr:k=>k,toast:text=>{message=text;},fetch:()=>{calls++;}};
  runInNewContext(source.slice(source.indexOf('async function save(){'),source.indexOf('// Status comes from the device')),ctx);
  await ctx.save();assert.equal(calls,0);assert.equal(message,'cfgFail');
});
function fixture(){
  const elements=new Map();let paints=0,clears=0;
  const ctx={Date:{now:()=>10000},LANG:'de',lastState:9000,frameBusy:false,AbortSignal,
    updateRestartVersion:'',updateText:key=>key,
    $:id=>{if(!elements.has(id))elements.set(id,{});return elements.get(id);},
    cx:{clearRect(){clears++;}},cv:{width:640,height:160},paint(){paints++;}};
  runInNewContext(source.slice(source.indexOf('async function frame()'),source.indexOf('// The mirror starts')),ctx);
  runInNewContext(source.slice(source.indexOf('function expireStatus()'),source.indexOf('setInterval(expireStatus')),ctx);
  return {ctx,elements,paintCount:()=>paints,clearCount:()=>clears};
}
test('disconnected browser clears readings and mirror after five seconds',()=>{
  const f=fixture();f.ctx.$('vHr').textContent='130';f.ctx.expireStatus();assert.equal(f.ctx.$('vHr').textContent,'130');
  f.ctx.lastState=5000;f.ctx.expireStatus();
  for(const id of ['vHr','vOx','vBat'])assert.equal(f.ctx.$(id).textContent,'--');
  assert.equal(f.ctx.$('dSleep').textContent,'unbekannt');assert.equal(f.ctx.$('pScreen').textContent,'Offline');assert.equal(f.clearCount(),1);
});
test('late or unsuccessful mirror response cannot repaint expired values',async()=>{
  const f=fixture();f.ctx.fetch=async()=>({ok:true,text:async()=> 'ab'.repeat(769)});
  await f.ctx.frame();assert.equal(f.paintCount(),1);
  f.ctx.lastState=0;await f.ctx.frame();assert.equal(f.paintCount(),1);
  f.ctx.lastState=9000;f.ctx.fetch=async()=>({ok:false});await f.ctx.frame();assert.equal(f.paintCount(),1);
});
test('mirror requests do not accumulate while the clock is slow',async()=>{
  const f=fixture();let release,calls=0;
  f.ctx.fetch=()=>{calls++;return new Promise(resolve=>{release=resolve;});};
  const first=f.ctx.frame();await f.ctx.frame();assert.equal(calls,1);
  release({ok:true,text:async()=> '00'.repeat(769)});await first;assert.equal(f.ctx.frameBusy,false);
});

function updateFixture(){
  const f=fixture();
  runInNewContext(source.slice(source.indexOf('const UPDATE_TEXT='),source.indexOf('async function ota()')),f.ctx);
  return f;
}
test('global update banner appears from device status and clears after installation',()=>{
  const f=updateFixture();
  f.ctx.renderUpdate({current:'1.0.10',latest:'1.0.11',available:true,phase:'available',busy:false});
  assert.equal(f.ctx.$('updateBanner').hidden,false);
  assert.equal(f.ctx.$('updateBannerTitle').textContent,'Update verfügbar: Version 1.0.11');
  assert.equal(f.ctx.$('updateBannerInstall').disabled,false);
  f.ctx.LANG='en';f.ctx.renderUpdate({current:'1.0.10',latest:'1.0.11',available:true,phase:'error',error:'network',busy:false});
  assert.equal(f.ctx.$('updateBannerTitle').textContent,'Update available: version 1.0.11');
  assert.match(f.ctx.$('updateBannerStatus').textContent,/Could not load/);
  f.ctx.renderUpdate({current:'1.0.11',latest:'1.0.11',available:false,phase:'current',busy:false});
  assert.equal(f.ctx.$('updateBanner').hidden,true);assert.equal(f.ctx.$('updateBannerInstall').disabled,true);
});
test('banner uses the existing single install request and shows progress or failure',async()=>{
  const f=updateFixture();let complete,calls=0;
  f.ctx.renderUpdate({current:'1.0.10',latest:'1.0.11',available:true,phase:'available',busy:false});
  f.ctx.fetch=(url,options)=>{calls++;assert.equal(url,'/api/update/install');assert.equal(options.method,'POST');return new Promise(resolve=>{complete=resolve;});};
  const installing=f.ctx.onlineUpdate(true);await f.ctx.onlineUpdate(true);
  assert.equal(calls,1);assert.equal(f.ctx.$('updateBannerInstall').disabled,true);
  complete({json:async()=>({current:'1.0.10',latest:'1.0.11',available:true,phase:'queued',busy:true})});await installing;
  f.ctx.renderUpdate({current:'1.0.10',latest:'1.0.11',available:true,phase:'downloading',busy:true,received:5,total:10});
  assert.match(f.ctx.$('updateBannerStatus').textContent,/50 %/);assert.equal(f.ctx.$('updateBannerInstall').disabled,true);
  f.ctx.renderUpdate({current:'1.0.10',latest:'1.0.11',available:true,phase:'error',error:'hash',busy:false});
  assert.match(f.ctx.$('updateBannerStatus').textContent,/Prüfsumme/);assert.equal(f.ctx.$('updateBannerInstall').disabled,false);assert.equal(calls,1);
});
test('online update shows current release progress and locks both flash actions',()=>{
  const f=updateFixture();f.ctx.renderUpdate({current:'1.0.4',latest:'1.0.5',phase:'downloading',received:512,total:1024,busy:true});
  assert.equal(f.ctx.$('updateCurrent').textContent,'1.0.4');assert.equal(f.ctx.$('updateLatest').textContent,'1.0.5');
  assert.equal(f.ctx.$('updateProgress').value,50);assert.equal(f.ctx.$('updateProgress').hidden,false);
  for(const id of ['updateInstall','updateCheck','manualInstall','fw'])assert.equal(f.ctx.$(id).disabled,true);
  assert.match(f.ctx.$('updateStatus').textContent,/50 %/);
});
test('update error remains explicit and allows a deliberate retry',()=>{
  const f=updateFixture();f.ctx.renderUpdate({current:'1.0.4',phase:'error',error:'hash',busy:false});
  assert.match(f.ctx.$('updateStatus').textContent,/Prüfsumme/);assert.equal(f.ctx.$('updateInstall').disabled,false);
});
test('one click sends one install request without automatic retry',async()=>{
  const f=updateFixture();let complete;const calls=[];
  f.ctx.fetch=(url,options)=>{calls.push({url,options});return new Promise(resolve=>{complete=resolve;});};
  const request=f.ctx.onlineUpdate(true);await f.ctx.onlineUpdate(true);assert.equal(calls.length,1);
  assert.equal(calls[0].url,'/api/update/install');assert.equal(calls[0].options.method,'POST');
  complete({json:async()=>({phase:'queued',busy:true})});await request;assert.equal(calls.length,1);
  assert.equal(f.ctx.$('updateInstall').disabled,true);
});
test('post-update reconnect shows installed version and never starts another update',()=>{
  const f=updateFixture();f.ctx.renderUpdate({current:'1.0.4',latest:'1.0.5',phase:'restarting',busy:true,received:100,total:100});
  f.ctx.renderUpdate({current:'1.0.5',phase:'idle',busy:false});
  assert.match(f.ctx.$('updateStatus').textContent,/Update erfolgreich: Version 1.0.5/);
  assert.equal(f.ctx.$('updateInstall').disabled,false);
});
test('lost install response displays uncertainty without retrying installation',async()=>{
  const f=updateFixture();let calls=0;f.ctx.fetch=async()=>{calls++;throw Error('timeout');};
  await f.ctx.onlineUpdate(true);assert.equal(calls,1);assert.match(f.ctx.$('updateStatus').textContent,/Uhr nicht erreichbar/);
});

test('unchecked version is explained and a check-only response shows the installed release',async()=>{
  const f=updateFixture();const calls=[];
  f.ctx.renderUpdate({current:'1.0.5',latest:'',phase:'idle',busy:false});
  assert.equal(f.ctx.$('updateLatest').textContent,'Noch nicht geprüft');
  f.ctx.fetch=async(url,options)=>{calls.push({url,options});return {json:async()=>({current:'1.0.5',latest:'',phase:'queued',busy:true})};};
  await f.ctx.onlineUpdate(false);
  assert.equal(calls.length,1);assert.equal(calls[0].url,'/api/update/check');assert.equal(calls[0].options.method,'POST');
  f.ctx.renderUpdate({current:'1.0.5',latest:'1.0.5',phase:'current',busy:false});
  assert.equal(f.ctx.$('updateLatest').textContent,'1.0.5');assert.equal(f.ctx.$('updateCheck').disabled,false);
  assert.match(f.ctx.$('updateStatus').textContent,/bereits installiert/);
});

test('reboot during a check is explained after reconnect and remains until retry',async()=>{
  const f=updateFixture();
  f.ctx.renderUpdate({current:'1.0.5',latest:'',phase:'checking',busy:true});
  const idle={current:'1.0.5',latest:'',phase:'idle',busy:false};
  f.ctx.renderUpdate(idle);f.ctx.renderUpdate(idle);
  assert.match(f.ctx.$('updateStatus').textContent,/vor Abschluss der Updateprüfung neu gestartet/);
  f.ctx.fetch=async()=>({json:async()=>({current:'1.0.5',phase:'queued',busy:true})});
  await f.ctx.onlineUpdate(false);assert.match(f.ctx.$('updateStatus').textContent,/angefordert/);
});

test('latest checked version stays visible alongside a failed check',()=>{
  const f=updateFixture();
  f.ctx.renderUpdate({current:'1.0.5',latest:'1.0.6',phase:'error',error:'network',busy:false});
  assert.equal(f.ctx.$('updateLatest').textContent,'1.0.6');assert.match(f.ctx.$('updateStatus').textContent,/konnte nicht geladen/);
});
