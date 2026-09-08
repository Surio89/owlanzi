// Local UI preview with illustrative data; never contacts a clock or Owlet.
// Run: node tools/preview-ui.mjs [port]. Reload to pick up webui.cpp edits.
import {createServer} from 'node:http';
import {readFileSync} from 'node:fs';

const read=name=>readFileSync(new URL('../src/'+name,import.meta.url),'utf8');
const header=read('owlanzi.h');
const palette=Object.fromEntries([...header.match(/struct Palette \{([\s\S]*?)\n\};/)[1].matchAll(/uint32_t\s+(\w+)\s*=\s*(0x[0-9A-F]+)/g)].map(m=>[m[1],Number(m[2])]));
const font=[...read('display.cpp').match(/FONT\[59\]\[3\] PROGMEM = \{([\s\S]*?)\n\};/)[1].matchAll(/0x([0-9A-F]{2})/g)].map(m=>m[1]).join('');
const config={...Object.fromEntries([...header.match(/struct Config \{([\s\S]*?)\n\};/)[1].matchAll(/(?:int|bool)\s+(\w+)\s*=\s*(\d+|true|false)/g)].map(m=>[m[1],m[2]==='true'?true:m[2]==='false'?false:Number(m[2])])),
  wifiSsid:'Zuhause',owletMail:'demo@example.com',owletDsn:'',europe:true,lang:'de',pal:{...palette},palDef:palette,font};
const version=header.match(/OWLANZI_VERSION\s+"([^"]+)"/)[1];
const state={version,screen:1,screenName:'Vitals',screenNameDe:'Vitalwerte',why:'fresh readings',whyDe:'frische Messwerte',bri:5,ldr:34,ambient:false,
  fresh:true,alarm:false,silenced:false,alarmText:'',hr:118,ox:98,bat:84,ss:15,sleepName:'deep sleep',sleepNameDe:'Tiefschlaf',chg:0,sockOff:false,bso:true,hw:'OSS 3',
  loggedIn:true,age:2,devices:'Demo Sock',polls:1248,fails:0,tok:2800,ssid:'Zuhause',rssi:-48,ip:'192.168.1.42',heap:192,block:108,up:18720,
  update:{current:version,latest:'',phase:'idle',busy:false}};
let previewUntil=0,previewBody=null;
function frame(){
 const pixels=Array(256).fill('000000');
 const px=(x,y,color)=>{if(x>=0&&x<32&&y>=0&&y<8)pixels[y*32+x]=color;};
 const text=(x,value,color)=>{for(const c of value){const start=(c.charCodeAt(0)-32)*6;for(let col=0;col<3;col++){const bits=parseInt(font.slice(start+col*2,start+col*2+2),16);for(let y=0;y<5;y++)if(bits&(1<<y))px(x+col,1+y,color);}x+=4;}};
 const preview=Date.now()<previewUntil;
 const pal=preview&&previewBody?.pal?previewBody.pal:config.pal;
 const color=key=>pal[key].toString(16).padStart(6,'0');
 const mode=preview?previewBody?.m:0;
 if(mode&&mode!==4){text(0,({5:'84%',6:'--- --',7:'OFFLINE',8:'ALARM',9:'INFO',1:'CORNERS',2:'CHASE',3:'COLOURS'})[mode]||'PREVIEW',color(mode===8?'alarm':mode===9?'info':'numbers'));}
 else{
  px(1,1,color('heart'));px(3,1,color('heart'));for(let x=0;x<5;x++){px(x,2,color('heart'));px(x,3,color('heart'));}for(let x=1;x<4;x++)px(x,4,color('heart'));px(2,5,color('heart'));
  text(7,String(state.hr),color('numbers'));px(20,3,color('sep'));text(24,String(state.ox),color('numbers'));for(let x=0;x<8;x++)px(x,7,color('deepSleep'));
 }
 return pixels.join('')+Number(config.briTest).toString(16).padStart(2,'0');
}
const requests=[];
createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 const json=(value,status=200)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(value));};
 let body='';for await(const chunk of req)body+=chunk;
 if(req.method==='POST'||url.pathname==='/api/test'||url.pathname==='/api/sound')requests.push({path:url.pathname,body:body?JSON.parse(body):null,query:url.search});
 if(url.pathname==='/__requests')return json(requests);
 if(url.pathname==='/__checks.js'){res.writeHead(200,{'Content-Type':'text/javascript'});return res.end(readFileSync(new URL('../tests/ui-browser-checks.mjs',import.meta.url),'utf8'));}
 if(url.pathname==='/__state'&&req.method==='POST'){Object.assign(state,JSON.parse(body));return json({ok:true});}
 if(url.pathname==='/api/config'){
  if(req.method==='POST'){const next=JSON.parse(body);Object.assign(config,next);previewUntil=0;return json({ok:true});}return json(config);
 }
 if(url.pathname==='/api/state')return json({...state,...(Date.now()<previewUntil?{screenName:'Preview',screenNameDe:'Vorschau',fresh:false,why:'preview running',whyDe:'Vorschau läuft'}:{})});
 if(url.pathname==='/api/frame'){res.writeHead(200,{'Content-Type':'text/plain'});return res.end(frame());}
 if(url.pathname==='/api/preview'){previewUntil=Date.now()+10000;previewBody=JSON.parse(body);return json({ok:true});}
 if(url.pathname==='/api/preview/stop'){previewUntil=0;return json({ok:true});}
 if(url.pathname==='/api/test'){previewBody={m:Number(url.searchParams.get('m'))};previewUntil=previewBody.m?Date.now()+25000:0;return json({ok:true});}
 if(url.pathname==='/api/scan')return json([{s:'Zuhause',r:-48,e:true},{s:'Gastnetz',r:-62,e:true}]);
 if(url.pathname==='/api/update/check'){state.update={current:version,latest:version,phase:'current',busy:false};return json(state.update);}
 if(url.pathname==='/api/update/install'){state.update={...state.update,phase:'queued',busy:true};return json(state.update);}
 if(url.pathname==='/api/hush'){state.silenced=true;return json({ok:true});}
 if(url.pathname==='/api/sound')return json({ok:true});
 if(url.pathname.startsWith('/api/'))return json({error:'Not simulated in the local preview'},400);
 const strings=Object.fromEntries([...read('webui.cpp').matchAll(/static const char (\w+)\[\] PROGMEM = R"(\w+)\(([\s\S]*?)\)\2";/g)].map(m=>[m[1],m[3]]));
 if(url.pathname==='/favicon.svg'){res.writeHead(200,{'Content-Type':'image/svg+xml'});return res.end(strings.LOGO);}
 if(!['/','/setup'].includes(url.pathname)){res.writeHead(404);return res.end();}
 const parts=url.pathname==='/setup'?['SETUP_PAGE','CSS','PW_JS','<div class=w><div style="padding:20px 0 6px" class=title><h1>','LOGO','SETUP_BODY']:['P_HEAD','CSS','PW_JS','P_A','LOGO','P_B','P_C','P_D','P_JS1','P_JS2','P_JS3'];
 res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});
 res.end(parts.map(key=>strings[key]??key).join(''));
}).listen(Number(process.argv[2]||4173),'127.0.0.1',()=>console.log(`Owlanzi preview with demo data: http://127.0.0.1:${process.argv[2]||4173}`));
