std::string sampleApp(){std::string app(4097,'A');app[0]=(char)0xe9;app[2]=2;app[3]=0x20;app[12]=app[13]=0;return app;}
std::string digest(const std::string &data){
  mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);assert(!mbedtls_sha256_starts_ret(&hash,0));
  assert(!mbedtls_sha256_update_ret(&hash,(const uint8_t*)data.data(),data.size()));uint8_t bytes[32];
  assert(!mbedtls_sha256_finish_ret(&hash,bytes));mbedtls_sha256_free(&hash);
  char hex[65];for(int i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",bytes[i]);return hex;
}
// Keep the simulated available release newer after each production version bump.
const std::string nextTestVersion=[](){
  std::string current=OWLANZI_VERSION;const auto patch=current.rfind('.')+1;
  return current.substr(0,patch)+std::to_string(std::stoi(current.substr(patch))+1);
}();
std::string onlineManifest(const char *version=nextTestVersion.c_str(),const std::string &app=sampleApp()){
  JsonDocument d;d["schema"]=1;d["target"]=updateTarget();d["chip"]="ESP32";d["kind"]="ota-app";
  d["layout"]="owlanzi-4m-v1";d["version"]=version;d["flash_size_bytes"]=4194304;
  d["file"]=std::string("owlanzi-")+updateTarget()+"-"+version+"-ota.bin";d["size_bytes"]=(uint32_t)app.size();d["sha256"]=digest(app);
  std::string out;serializeJson(d,out);return out;
}
bool parseManifest(const std::string &s){JsonDocument d;deserializeJson(d,s);OnlineRelease r;return parseOnlineRelease(d,r);}
void noticeFrame(uint32_t elapsed=100){fakeNow+=elapsed;gSt.lastOkAt=fakeNow;dispTick();}
void testUpdateNotices(){
  reset();fresh();replies.push_back({200,onlineManifest()});onlineUpdateRequest(false);onlineUpdateTick();noticeFrame();
  check("real available release arms a notice without covering vital readings",gSt.screen==SCR_VITALS&&!gSt.updateNotice&&gUpdateNoticeRemaining==20000);
  noticeFrame(30000);check("notice waits without timing out before Battery",!gSt.updateNotice&&gUpdateNoticeRemaining==20000);
  gSt.v.charging=1;noticeFrame();
  check("notice starts only on the real Battery screen at normal brightness",gSt.screen==SCR_BATTERY&&gSt.updateNotice&&gUpdateNoticeRemaining==20000&&dispBrightness()!=BRI_FULL&&!gMsgActive);
  noticeFrame(5000);check("notice accounts for five seconds of actual display time",gUpdateNoticeRemaining==15000);
  noticeFrame(60);check("notice scroll progresses across ordinary Battery frames",gUpdateNoticeScrollX<MATRIX_W);
  gSt.v.charging=0;noticeFrame();uint32_t paused=gUpdateNoticeRemaining;
  check("return to Vitals hides the notice immediately",gSt.screen==SCR_VITALS&&!gSt.updateNotice);
  noticeFrame(30000);check("time on another screen does not consume the remaining notice",gUpdateNoticeRemaining==paused);
  gSt.v.sockOff=true;noticeFrame();check("sock-off Battery resumes remaining notice",gSt.screen==SCR_BATTERY&&gSt.updateNotice&&gUpdateNoticeRemaining==paused);
  gAlarmCritical=true;gAlarmText="ALARM";noticeFrame();paused=gUpdateNoticeRemaining;
  check("critical alarm preempts the Battery update notice",gSt.screen==SCR_ALARM&&!gSt.updateNotice);
  noticeFrame(5000);check("alarm does not spend the remaining notice time",gUpdateNoticeRemaining==paused);
  gAlarmCritical=false;gAlarmText="";noticeFrame();noticeFrame(paused-1);
  check("notice remains visible until its twentieth displayed second",gSt.updateNotice&&gUpdateNoticeRemaining==1);
  noticeFrame(33);check("after twenty seconds the Battery display returns",gSt.screen==SCR_BATTERY&&!gSt.updateNotice&&gUpdateNoticeRemaining==0);
  replies.push_back({200,onlineManifest()});onlineUpdateRequest(false);onlineUpdateTick();noticeFrame();
  check("repeated checks of the same available version do not rearm the notice",!gSt.updateNotice&&gUpdateNoticeRemaining==0);
  dispUpdateAvailable("9.0.0");noticeFrame();check("a different available version gets its own notice",gSt.updateNotice&&gUpdateNoticeRemaining==20000);
  replies.push_back({200,onlineManifest(OWLANZI_VERSION)});onlineUpdateRequest(false);onlineUpdateTick();noticeFrame();
  check("manifest with no newer release clears the pending notice",!gSt.updateNotice&&gUpdateNoticeRemaining==0);
  for(int mode=0;mode<6;++mode) {
    reset();fresh();gSt.v.charging=1;dispUpdateAvailable(nextTestVersion.c_str());
    if(mode==0)gSt.wifiOk=false;
    if(mode==1)gSt.apMode=true;
    if(mode==2)dispTest(TEST_BATTERY,20);
    if(mode==3)dispMessage("IP 127.0.0.1",20,0xffffff);
    if(mode==4)gAlarmText="LOW BATTERY";
    if(mode==5)updateClaim(UpdateOwner::MANUAL);
    noticeFrame();check("offline setup previews messages alarms and installation keep priority",!gSt.updateNotice&&gUpdateNoticeRemaining==20000);
  }
  reset();fresh();gSt.v.charging=1;fakeNow=0xfffffff0;dispUpdateAvailable(nextTestVersion.c_str());noticeFrame(0);
  noticeFrame(19999);check("Battery notice remains visible across millis rollover",gSt.updateNotice&&gUpdateNoticeRemaining==1);
  noticeFrame(33);check("Battery notice expires correctly across millis rollover",!gSt.updateNotice&&gUpdateNoticeRemaining==0);
}
void testOnlineUpdates(){
  testUpdateNotices();
  reset();
  const std::string dailyUrl=std::string("https://owlanzi.com/firmware/ota-")+updateTarget()+".json?daily-update-check=1";
  replies.push_back({200,onlineManifest()});
  check("daily scheduler performs a real check without installing",onlineUpdateTick()&&requestedUrls.size()==1&&requestedUrls[0]==dailyUrl&&!Update.begins&&!ESP.restarts&&std::string(phase)=="available");
  check("same-day idle tick performs no request",!onlineUpdateTick()&&requestedUrls.size()==1);
  const uint32_t persisted=savedDailyDay;
  dailyLoaded=false;lastDailyDay=0; // simulate volatile state lost on reboot
  check("reboot retains the daily reservation",!onlineUpdateTick()&&savedDailyDay==persisted&&requestedUrls.size()==1);
  fakeEpoch+=86400;replies.push_back({200,onlineManifest()});
  check("next day schedules one new check",onlineUpdateTick()&&requestedUrls.size()==2&&savedDailyDay==persisted+1);
  fakeEpoch-=86400;
  check("clock moving backwards cannot duplicate an earlier day",!onlineUpdateTick());
  reset();replies.push_back({-1,"",[]{check("day persisted before the network attempt",savedDailyDay>0);}});
  onlineUpdateTick();dailyLoaded=false;lastDailyDay=0;
  check("lost response is not retried with another daily marker",!onlineUpdateTick()&&requestedUrls.size()==1);
  replies.push_back({200,onlineManifest()});onlineUpdateRequest(false);onlineUpdateTick();
  check("manual retry after daily failure remains unmarked",requestedUrls.size()==2&&requestedUrls.back().find('?')==std::string::npos);
  reset();dailyWriteOk=false;replies.push_back({200,onlineManifest()});onlineUpdateTick();
  check("NVS failure keeps update check working without counting",savedDailyDay==0&&requestedUrls.size()==1&&requestedUrls[0].find('?')==std::string::npos&&!onlineUpdateTick());
  reset();savedDailyDay=dailyCheckDay(fakeEpoch);dailyReadOk=false;replies.push_back({200,onlineManifest()});onlineUpdateTick();
  check("failed initial NVS read cannot overwrite and recount an existing day",requestedUrls.size()==1&&requestedUrls[0].find('?')==std::string::npos);
  for(int mode=0;mode<6;++mode) {
    reset();if(mode==0)gCfg.autoUpdateCheck=false;if(mode==1)gSt.wifiOk=false;if(mode==2)gSt.apMode=true;
    if(mode==3)gAlarmCritical=true;if(mode==4)fakeEpoch=0;if(mode==5)fakeNow=1000;
    check("daily check waits for enabled setting, WiFi, clock, alarm and boot grace",!onlineUpdateTick()&&requestedUrls.empty()&&savedDailyDay==0);
    gCfg.autoUpdateCheck=true;gSt.wifiOk=true;gSt.apMode=false;gAlarmCritical=false;fakeEpoch=parseUtc("2026-09-07T10:00:00Z");fakeNow=60000;
    replies.push_back({200,onlineManifest()});check("deferred daily check runs once preconditions recover",onlineUpdateTick()&&requestedUrls[0]==dailyUrl);
  }
  reset();updateClaim(UpdateOwner::MANUAL);
  check("daily scheduler cannot interrupt a manual upload",!onlineUpdateTick()&&requestedUrls.empty());
  for(const char* midnight:{"2026-01-14T23:00:00Z","2026-07-14T22:00:00Z","2026-03-28T23:00:00Z","2026-03-29T22:00:00Z","2026-10-24T22:00:00Z","2026-10-25T23:00:00Z"}) {
    time_t instant=parseUtc(midnight);
    check("daily boundary follows Berlin winter and summer midnight",dailyCheckDay(instant)==dailyCheckDay(instant-1)+1);
  }
  for(const char* transition:{"2026-03-29T01:00:00Z","2026-10-25T01:00:00Z"}) {
    time_t instant=parseUtc(transition);
    check("daylight-saving transition does not create a second day",dailyCheckDay(instant)==dailyCheckDay(instant-1));
  }
  reset();config(R"({"autoUpdateCheck":false})");
  check("daily checks can be disabled through the real configuration handler",srv.status==200&&!gCfg.autoUpdateCheck&&!onlineUpdateTick());
  reset();strlcpy(gCfg.webPass,"fixture",sizeof(gCfg.webPass));srv.auth=false;handleOnlineUpdate(true);
  check("unauthorized online update cannot queue a download",srv.status==401&&!queued&&!updateBusy());
  reset();srv.origin="https://unrelated.test";handleOnlineUpdate(true);
  check("cross-origin online update cannot queue a download",srv.status==403&&!queued&&!updateBusy());
  reset();srv.origin="http://owlanzi.local";handleOnlineUpdate(true);
  check("same-origin online install returns accepted before network work",srv.status==202&&queued&&requestedUrls.empty());
  check("SHA256 host adapter uses a real cryptographic hash",digest("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  check("semantic versions compare numerically",newerVersion("1.0.10","1.0.9")&&newerVersion("2.0.0","1.9.99")&&!newerVersion("1.0.4","1.0.4")&&!newerVersion("1.0.3","1.0.4"));
  for(const char *bad:{"1","1.2","1.2.3.4","1.2.3-rc1","1.2.3/../x","999999.0.0","x.0.0"})check("malformed release version rejected",!newerVersion(bad,"0.0.0"));
  check("valid online release description accepted",parseManifest(onlineManifest()));
  for(const char *field:{"schema","target","kind","chip","layout","flash_size_bytes","file","sha256","size_bytes","version"}){
    JsonDocument d;deserializeJson(d,onlineManifest());d[field]="invalid";std::string text;serializeJson(d,text);
    check("invalid online release metadata rejected",!parseManifest(text));
  }
  reset();replies.push_back({200,onlineManifest()});check("check request queued asynchronously",onlineUpdateRequest(false)&&updateBusy()&&replies.size()==1);
  check("duplicate online request rejected",!onlineUpdateRequest(true));onlineUpdateTick();check("check downloads no application",std::string(phase)=="available"&&!updateBusy()&&Update.begins==0&&ESP.restarts==0);
  check("HTTPS certificate validation configured",certificateConfigured);
  reset();fakeTlsError=49;replies.push_back({200,onlineManifest()});onlineUpdateRequest(false);onlineUpdateTick();
  check("positive secure-client success result is not reported as a TLS error",lastTlsError==0&&lastHttpStatus==200&&std::string(phase)=="available");
  check("TLS I/O and handshake limits use seconds",secureTimeoutSeconds==15&&handshakeTimeoutSeconds==15);
  JsonDocument checked;onlineUpdateJson(checked.to<JsonObject>());
  check("successful check reports latest version to the WebUI",checked["latest"]==nextTestVersion&&checked["available"]==true&&checked["http_status"]==200);
  check("metadata uses the fixed HTTPS target URL",requestedUrls.size()==1&&requestedUrls[0]==std::string("https://owlanzi.com/firmware/ota-")+updateTarget()+".json");
  replies.push_back({500,"temporary failure"});onlineUpdateRequest(false);
  onlineUpdateJson(checked.to<JsonObject>());check("retry keeps the last checked version visible",checked["latest"]==nextTestVersion);
  onlineUpdateTick();onlineUpdateJson(checked.to<JsonObject>());
  check("failed retry retains latest version and reports the failure",checked["latest"]==nextTestVersion&&checked["phase"]=="error"&&checked["http_status"]==500&&!ESP.restarts&&!Update.begins);
  for(const char *version:{OWLANZI_VERSION,"1.0.4"}){
    reset();replies.push_back({200,onlineManifest(version)});onlineUpdateRequest(true);onlineUpdateTick();
    check("current and older releases never flash",std::string(phase)=="current"&&!updateBusy()&&!Update.begins&&!ESP.restarts);
    onlineUpdateJson(checked.to<JsonObject>());check("current and older release versions remain visible",checked["latest"]==version);
  }
  reset();replies.push_back({200,onlineManifest(OWLANZI_VERSION)});onlineUpdateRequest(false);onlineUpdateTick();
  onlineUpdateJson(checked.to<JsonObject>());check("check of installed release shows its version without rebooting",checked["latest"]==OWLANZI_VERSION&&checked["phase"]=="current"&&!ESP.restarts&&!Update.begins);
  reset();fakeTlsError=-0x2700;replies.push_back({-1,""});onlineUpdateRequest(false);onlineUpdateTick();
  onlineUpdateJson(checked.to<JsonObject>());check("TLS failure returns diagnostics without rebooting",checked["tls_error"]==-0x2700&&checked["phase"]=="error"&&!ESP.restarts);
  for(bool image:{false,true}){
    reset();if(image)replies.push_back({200,onlineManifest()});
    replies.push_back({200,image?sampleApp():onlineManifest(),{},-2,false,1024,true});onlineUpdateRequest(image);onlineUpdateTick();
    check("closed HTTP stream is handled without dereferencing null",std::string(errorCode)=="incomplete"&&!ESP.restarts&&!Update.begins&&!updateBusy());
  }
  reset();strlcpy(gCfg.wifiSsid,"personal-network",sizeof(gCfg.wifiSsid));strlcpy(gCfg.owletMail,"user@example.test",sizeof(gCfg.owletMail));gCfg.pal.heart=0x123456;gCfg.spo2Limit=88;cfgSave();
  Config saved=gCfg;int writes=preferenceWrites;replies.push_back({200,onlineManifest()});replies.push_back({200,sampleApp(),{},-2,false,7});
  onlineUpdateRequest(true);onlineUpdateTick();
  check("complete online image restarts once after verification",ESP.restarts==1&&Update.ends==1&&std::string(phase)=="restarting");
  check("only application bytes were written",Update.written==sampleApp()&&Update.expected==sampleApp().size());
  check("user settings and NVS writes are untouched by online update",memcmp(&saved,&gCfg,sizeof(Config))==0&&preferenceWrites==writes);
  check("successful update reserves flash ownership until restart",updateOwnedBy(UpdateOwner::ONLINE)&&!updateClaim(UpdateOwner::MANUAL));
  check("small transport chunks assemble without data loss",received==sampleApp().size());
  reset();onlineUpdateRequest(true);int before=Update.aborts;upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE);upload(UPLOAD_FILE_END);handleOtaFinish();
  check("manual request cannot abort an online download",updateOwnedBy(UpdateOwner::ONLINE)&&Update.aborts==before&&srv.status==409);
  reset();upload(UPLOAD_FILE_START);check("online request cannot interrupt manual flash",!onlineUpdateRequest(true)&&updateOwnedBy(UpdateOwner::MANUAL));upload(UPLOAD_FILE_ABORTED);check("manual abort releases ownership",!updateBusy());
  reset();onlineUpdateRequest(true);saved=gCfg;config(R"({"wifiSsid":"changed"})");check("configuration cannot reboot device during update",srv.status==409&&memcmp(&saved,&gCfg,sizeof(Config))==0&&!ESP.restarts);
  for(int mode=0;mode<3;++mode){
    reset();if(mode==0)gSt.wifiOk=false;if(mode==1)fakeEpoch=0;if(mode==2)gAlarmCritical=true;
    check("WiFi time and alarm preconditions checked",!onlineUpdateRequest(true)&&!updateBusy());
  }
  for(int mode=0;mode<4;++mode){
    reset();if(mode==0)nvsPart.address=0xa000;if(mode==1)app1Part.size=0x100000;if(mode==2)ESP.flashSize=8388608;if(mode==3)app1Part.address=app0Part.address;
    replies.push_back({200,onlineManifest()});onlineUpdateRequest(true);onlineUpdateTick();
    check("incompatible layout rejected before image fetch or flash",std::string(errorCode)=="layout"&&!updateBusy()&&!Update.begins&&requestedUrls.size()==1);
  }
  for(int code:{-1,301,404,500}){
    reset();replies.push_back({code,"failure"});onlineUpdateRequest(true);onlineUpdateTick();
    check("network HTTP and redirect failures do not flash",std::string(errorCode)=="network"&&!updateBusy()&&!Update.begins&&!ESP.restarts);
  }
  for(int length:{-1,0,2049}){
    reset();replies.push_back({200,onlineManifest(),{},length});onlineUpdateRequest(true);onlineUpdateTick();
    check("unknown and oversized manifest bodies rejected",std::string(errorCode)=="manifest"&&!updateBusy()&&!Update.begins);
  }
  reset();replies.push_back({200,"not json"});onlineUpdateRequest(true);onlineUpdateTick();check("malformed JSON cannot start a flash",std::string(errorCode)=="manifest"&&!Update.begins);
  for(int mode=0;mode<8;++mode){
    reset();replies.push_back({200,onlineManifest()});Reply image{200,sampleApp()};
    if(mode==0)image.body[200]='B'; // checksum mismatch
    if(mode==1)image.length=100; // wrong content length
    if(mode==2){image.length=(int)image.body.size();image.body.resize(1048);} // truncated stream after first write
    if(mode==3)image.body[0]=0; // merged image / invalid magic
    if(mode==4)image.body[3]=0x30; // eight MB app
    if(mode==5)Update.writeOk=false;
    if(mode==6)Update.endOk=false;
    if(mode==7)image.stall=true;
    replies.push_back(image);onlineUpdateRequest(true);onlineUpdateTick();
    check("download or flash failure keeps current firmware running",!ESP.restarts&&!updateBusy()&&!Update.running&&std::string(phase)=="error");
    if(mode!=6)check("invalid/incomplete image never selects boot partition",Update.ends==0);
  }
  reset();replies.push_back({200,onlineManifest()});replies.push_back({200,sampleApp(),[]{gAlarmCritical=true;}});onlineUpdateRequest(true);onlineUpdateTick();check("alarm during request interrupts before flash",std::string(errorCode)=="interrupted"&&!Update.begins&&!ESP.restarts);
  reset();onlineUpdateRequest(true);gAlarmCritical=true;onlineUpdateTick();check("alarm while queued cancels before any network request",requestedUrls.empty()&&!updateBusy()&&std::string(errorCode)=="interrupted");
  reset();fakeNow=0xfffffff0;replies.push_back({200,onlineManifest()});replies.push_back({200,sampleApp(),{},-2,false,17});onlineUpdateRequest(true);onlineUpdateTick();check("online install works across millis rollover",ESP.restarts==1);
  reset();replies.push_back({500,"temporary failure"});onlineUpdateRequest(true);onlineUpdateTick();
  replies.push_back({200,onlineManifest()});replies.push_back({200,sampleApp()});onlineUpdateRequest(true);onlineUpdateTick();
  check("explicit retry after failure installs successfully",ESP.restarts==1&&std::string(phase)=="restarting");
}
