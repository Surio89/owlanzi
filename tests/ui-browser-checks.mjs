// Browser integration checks for the LOCAL preview server only.
// Start `node tools/preview-ui.mjs`, open http://127.0.0.1:4173/, then run:
// await (await import('/__checks.js')).checkUI()
// Repeat at desktop/tablet/phone widths. No browser library is required.
export async function checkUI(){
 if(location.hostname!=='127.0.0.1')throw Error('Only run against tools/preview-ui.mjs');
 const results=[];
 const assert=(condition,message)=>{if(!condition)throw Error(message);};
 const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
 const $=id=>document.getElementById(id);
 const requestLog=()=>fetch('/__requests').then(r=>r.json());
 const click=i=>$('tab'+i).click();
 const until=async(predicate)=>{for(let n=0;n<35;n++){if(await predicate())return;await wait(100);}throw Error('Timed out waiting for UI');};
 const record=name=>results.push('PASS '+name);
 await until(()=>$('c_heart').value!=='#000000'&&$('deviceVersion').textContent.startsWith('v'));
 const before=await requestLog();
 const ids=[...document.querySelectorAll('[id]')].map(e=>e.id);
 assert(new Set(ids).size===ids.length,'Duplicate element IDs');
 for(const lang of ['en','de']){
  document.querySelector('.sidebar-foot [data-l='+lang+']').click();
  await until(()=>document.documentElement.lang===lang);
  for(let i=0;i<4;i++){
   click(i);
   assert(document.querySelectorAll('main>section.on').length===1,'Exactly one visible panel');
   assert($('s'+i).classList.contains('on'),'Selected panel visible');
   assert(location.hash==='#'+['status','display','alarms','system'][i],'Deep link follows navigation');
   assert($('pageTitle').textContent===$('tab'+i).textContent,'Heading follows translated tab');
   assert($('tab'+i).getAttribute('aria-selected')==='true','Selected tab is accessible');
   assert(document.querySelectorAll('.tabs button[tabindex="0"]').length===1,'Roving tab focus');
   assert(document.documentElement.scrollWidth<=innerWidth,'No horizontal page overflow');
   const bounds=$('s'+i).getBoundingClientRect();
   assert(bounds.left>=0&&bounds.right<=innerWidth,'Panel stays within viewport');
   assert(!$('s'+i).querySelector('details,summary'),'No collapsible settings remain');
   for(const group of $('s'+i).querySelectorAll('.settings-group')){
    const body=group.querySelector('.group-body');
    assert(body.offsetHeight>0,'Every topic body is visible');
    group.querySelector('.group-heading').click();
    assert(body.offsetHeight>0,'Clicking a heading cannot collapse its contents');
   }
   const grid=$('s'+i).querySelector('.section-grid');
   assert(getComputedStyle(grid).gridTemplateColumns.split(' ').length===1,'Topics stay in one column');
   if(innerWidth>900){
    const sidebar=document.querySelector('.app-header').getBoundingClientRect(),main=document.querySelector('main').getBoundingClientRect();
    assert(sidebar.left===0,'Sidebar is anchored to the viewport edge');
    assert(main.width<=960,'Content remains at a readable width on ultrawide monitors');
    assert(Math.abs((main.left+main.right)/2-(sidebar.right+document.documentElement.clientWidth)/2)<2,'Content is centred in the area beside the sidebar');
    assert(document.scrollingElement===document.documentElement&&getComputedStyle(document.querySelector('main')).overflowY==='visible','Document keeps the scrollbar at the window edge');
   }
   if(i){const bar=$('s'+i).querySelector('.savebar').getBoundingClientRect();assert(bar.top>=0&&bar.bottom<=innerHeight,'Save actions are visible without scrolling');}
  }
 }
 record('All four panels, both languages, deep links, no overflow, visible save actions');
 click(0);$('tab0').focus();
 const vertical=document.querySelector('.tabs').getAttribute('aria-orientation')==='vertical';
 $('tab0').dispatchEvent(new KeyboardEvent('keydown',{key:vertical?'ArrowDown':'ArrowRight',bubbles:true}));
 assert(document.activeElement===$('tab1')&&$('s1').classList.contains('on'),'Arrow key moves focus and panel');
 $('tab1').dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}));
 assert(document.activeElement===$('tab3'),'End selects last tab');
 $('tab3').dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));
 assert(document.activeElement===$('tab0'),'Home selects first tab');
 click(1);$('c_heart').focus();await wait(300);
 const after=await requestLog();
 assert(after.slice(before.length).every(r=>r.path==='/api/config'&&Object.keys(r.body).join()==='lang'),'Navigation and focus do not change device display');
 record('Keyboard navigation and colour focus have no preview side effects');
 for(const input of document.querySelectorAll('main input:not([type=file]),main select')){
  assert(input.labels?.length||input.getAttribute('aria-labelledby'),'Missing label for '+input.id);
 }
 record('All settings controls have associated labels');
 const initialHeart=$('c_heart').value;
 $('c_heart').value='#228899';$('c_heart').dispatchEvent(new Event('input',{bubbles:true}));
 await until(async()=>(await requestLog()).some(r=>r.path==='/api/preview'&&r.body.pal.heart===0x228899));
 click(3);click(1);
 assert($('c_heart').value==='#228899','Unsaved colour survives switching tabs');
 record('Changing a colour sends a live preview');
 click(2);const toggle=$('ownAlarms'),armed=toggle.checked;
 if(toggle.checked)toggle.click();
 assert($('spo2Limit').disabled,'Disarmed alarm fields are disabled');
 toggle.click();assert(!$('spo2Limit').disabled,'Armed alarm fields can be used');
 if(toggle.checked!==armed)toggle.click();
 record('Alarm switches gate their settings');
 click(1);const min=$('briMin'),initialMin=min.value;
 min.value='1';min.dispatchEvent(new Event('input',{bubbles:true}));assert(!$('briWarn').hidden,'Low brightness warning is visible');
 min.value=initialMin;min.dispatchEvent(new Event('input',{bubbles:true}));
 assert($('briWarn').hidden===(Number(initialMin)>=5),'Brightness warning tracks slider');
 const count=(await requestLog()).length;
 document.querySelector('#s1 [data-i18n=saveDisplay]').click();
 await until(async()=>(await requestLog()).slice(count).some(r=>r.path==='/api/config'&&r.body.pal));
 const saved=(await requestLog()).slice(count).find(r=>r.path==='/api/config'&&r.body.pal).body;
 assert(saved.pal.heart===0x228899&&saved.briMin===Number(initialMin),'Save contains edited colour and current brightness');
 assert(!('wifiPass'in saved)&&!('owletPass'in saved)&&!('webPass'in saved),'Empty passwords are not overwritten');
 assert(saved.ownAlarms===armed,'Switch state is saved');
 $('c_heart').value=initialHeart;$('c_heart').dispatchEvent(new Event('input',{bubbles:true}));
 document.querySelector('#s1 [data-i18n=saveDisplay]').click();
 await until(()=>document.querySelector('#toast.on'));
 record('Save preserves settings and omits empty passwords');
 click(3);const eye=$('wifiPass').parentElement.querySelector('button');eye.click();assert($('wifiPass').type==='text','Password eye reveals input');eye.click();assert($('wifiPass').type==='password','Password eye hides input');
 const updateBefore=(await requestLog()).length;$('updateCheck').click();
 await until(()=>$('updateLatest').textContent===$('updateCurrent').textContent);
 const updateRequests=(await requestLog()).slice(updateBefore);
 assert(updateRequests.some(r=>r.path==='/api/update/check')&&!updateRequests.some(r=>r.path==='/api/update/install'),'Update check never installs');
 record('Password visibility and check-only firmware action');
 click(2);if(!toggle.checked)toggle.click();
 const limit=$('spo2Limit'),originalLimit=limit.value;limit.value='20';
 click(3);
 const invalidBefore=(await requestLog()).length;
 document.querySelector('#s3 [data-i18n=saveSystem]').click();await wait(150);
 assert($('s2').classList.contains('on')&&limit.offsetHeight>0,'Invalid field reveals its tab');
 assert(document.activeElement===limit,'Validation focuses the field that needs correction');
 assert((await requestLog()).length===invalidBefore,'Invalid settings are not sent');
 limit.value=originalLimit;if(toggle.checked!==armed)toggle.click();
 record('Validation reveals invalid fields in other tabs before saving');
 click(0);
 return {viewport:[innerWidth,innerHeight],results};
}
