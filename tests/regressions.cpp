// Compiled against production config/display/vitals/owlet and the real web
// handlers. Only hardware, network transport, clock, NVS and HTTP server are
// replaced. These tests never contact Owlet and use no private credentials.
static int passed=0,failed=0;
void check(const char*name,bool condition){printf("%s %s\n",condition?"PASS":"FAIL",name);condition?++passed:++failed;}
void reset(){
  gCfg=Config{};gCfg.soundEnabled=false;gSt=State{};gSt.wifiOk=true;
  gAlarmText=String();gAlarmCritical=false;gMsgActive=false;gPrevOn=false;
  fakeNow+=100000;fakeEpoch=parseUtc("2026-09-07T10:00:00Z");srv=ServerStub{};ESP.restarts=0;
  Update=UpdateStub{};otaStarted=otaOk=false;otaReceived=0;soundStop();replies.clear();requestedUrls.clear();
  owner=UpdateOwner::NONE;queued=false;latest=OnlineRelease{};phase="idle";errorCode="";received=0;
  dailyRequested=dailyLoaded=false;lastDailyDay=savedDailyDay=0;dailyReadOk=dailyWriteOk=true;
  gUpdateNoticeVersion[0]=0;gUpdateNoticeRemaining=gUpdateNoticeAt=0;gUpdateNoticeScrollX=MATRIX_W;gUpdateNoticeScrollAt=0;
  lastHttpStatus=lastTlsError=0;stackFreeMin=0;fakeTlsError=0;secureTimeoutSeconds=handshakeTimeoutSeconds=0;
  nvsPart={0x9000,0x5000};app1Part={0x1f0000,0x1e0000};ESP.flashSize=4194304;certificateConfigured=false;
}
std::string properties(int hr=130,int ox=98,int ss=8,int charging=0,const char*stamp="2026-09-07T10:00:00Z",const char*alert="",const char*value="1"){
  JsonDocument d;
  auto p=d[0]["property"];p["name"]="REAL_TIME_VITALS";p["data_updated_at"]=stamp;
  char raw[180];snprintf(raw,sizeof(raw),"{\"hr\":%d,\"ox\":%d,\"ss\":%d,\"bat\":90,\"bso\":true,\"sc\":1,\"chg\":%d}",hr,ox,ss,charging);
  p["value"]=raw;d[1]["property"]["name"]=alert;d[1]["property"]["value"]=value;
  std::string out;serializeJson(d,out);return out;
}
void poll(const std::string&props){JsonDocument d;deserializeJson(d,props);Vitals v;char err[96];assert(parseVitals(d,v,err,sizeof(err)));acceptVitals(v,true);}
void fresh(int ox=98){poll(properties());fakeNow+=5000;fakeEpoch+=5;poll(properties(130,ox,8,0,"2026-09-07T10:00:05Z"));}
void sample(int seconds,int ox=80){fakeNow+=5000;fakeEpoch=parseUtc("2026-09-07T10:00:00Z")+seconds;gSt.v.measuredAt=(uint32_t)fakeEpoch;gSt.v.oxygen=ox;gSt.lastOkAt=fakeNow;alarmsEvaluate(true);}
void config(const char*json){srv.body=json;handleConfigPost();}
void readyNetwork(){strlcpy(gCfg.owletMail,"fixture@example.test",sizeof(gCfg.owletMail));gSt.loggedIn=true;strlcpy(gSt.dsn,"fixture",sizeof(gSt.dsn));beginRequest();setTokenLifetime(3600);}
void upload(int status){srv.up.status=status;srv.up.currentSize=128;handleOtaUpload();}
void offlineFrame(uint32_t elapsed=40){fakeNow+=elapsed;stateTick();dispTick();}
void testOfflineGrace(){
  reset();offlineFrame();check("no offline grace before the first fetch",gSt.screen==SCR_OFFLINE);
  reset();fresh();offlineFrame();check("healthy connection shows vitals",gSt.screen==SCR_VITALS);
  gSt.cloudOk=false;alarmsEvaluate(false);offlineFrame();
  check("single failed fetch shows dashes without treating readings as fresh",gSt.screen==SCR_WAITING&&!cloudFresh()&&!vitalsFresh());
  check("reconnection reason in both UI languages",std::string(whyEn())=="waiting for connection to recover"&&std::string(whyDe())=="warte auf Wiederherstellung der Verbindung");
  poll(properties(130,98,8,0,"2026-09-07T10:00:05Z"));offlineFrame();
  check("successful retry restores vitals immediately",gSt.screen==SCR_VITALS&&vitalsFresh());
  gSt.cloudOk=false;
  for(int i=0;i<5;++i){gSt.lastTryAt=fakeNow;++gSt.failCount;offlineFrame(5000);}
  check("repeated failures remain waiting inside grace",gSt.screen==SCR_WAITING);
  offlineFrame(5000);check("retries cannot postpone offline past thirty seconds",gSt.screen==SCR_OFFLINE);
  check("confirmed outage reason in both UI languages",std::string(whyEn())=="Wi-Fi or cloud fetch unavailable"&&std::string(whyDe())=="WLAN oder Cloud-Abruf nicht verfügbar");
  poll(properties(130,98,8,0,"2026-09-07T10:00:05Z"));offlineFrame();
  check("successful fetch clears confirmed offline immediately",gSt.screen==SCR_VITALS);
  gSt.cloudOk=false;offlineFrame();check("new outage receives a new bounded grace",gSt.screen==SCR_WAITING);

  reset();fresh();offlineFrame(20001);
  check("stalled fetch still expires readings after twenty seconds",gSt.screen==SCR_WAITING&&!vitalsFresh());
  offlineFrame(9949);check("waiting just before thirty-second boundary",gSt.screen==SCR_WAITING);
  offlineFrame(50);check("offline at exactly thirty seconds since last fetch",gSt.screen==SCR_OFFLINE);
  reset();fresh();gSt.wifiOk=false;gSt.cloudOk=false;offlineFrame();
  check("brief WiFi loss hides readings without offline label",gSt.screen==SCR_WAITING&&!vitalsFresh());
  gSt.wifiOk=true;offlineFrame();check("WiFi recovery alone cannot restore old readings",gSt.screen==SCR_WAITING&&!vitalsFresh());

  reset();poll(properties(130,98,8,1));gSt.cloudOk=false;offlineFrame();
  check("charging status is not retained as current during reconnection",gSt.screen==SCR_WAITING);
  reset();fresh();gSt.v.criticalOx=true;gSt.cloudOk=false;alarmsEvaluate(false);offlineFrame();
  check("critical alarm and stale annotation remain immediate during grace",!cloudOffline()&&gSt.screen==SCR_ALARM&&std::string(gAlarmText.c_str()).find("OFFLINE")!=std::string::npos);
  reset();fresh();cloudInvalidate();offlineFrame();
  check("account change cannot borrow previous fetch grace",gSt.screen==SCR_OFFLINE);

  reset();fakeNow=0;poll(properties());gSt.cloudOk=false;offlineFrame();
  check("successful fetch at millis zero still grants grace",gSt.screen==SCR_WAITING);
  reset();fakeNow=0xfffffff0;poll(properties());gSt.cloudOk=false;offlineFrame(29950);
  check("offline grace survives millis rollover",gSt.screen==SCR_WAITING);
  offlineFrame(50);check("offline grace expires across millis rollover",gSt.screen==SCR_OFFLINE);
}
void testOnlineUpdates();
int main(){
  stateBegin();reset();
  testOfflineGrace();reset();
  for(auto entry:std::vector<std::pair<int,const char*>>{{0,"unknown"},{1,"awake"},{8,"light sleep"},{15,"deep sleep"},{42,"unknown"}})
    check(entry.second,strcmp(sleepName(entry.first,false),entry.second)==0);
  check("German light sleep",strcmp(sleepName(8,true),"leichter Schlaf")==0);
  fresh();check("parser preserves ss8",gSt.v.sleepSt==8);screenVitals(gSt.v);
  check("ss8 matrix blue bar",leds[xy(11,7)].b==C(gCfg.pal.lightSleep).b && leds[xy(11,7)].r==C(gCfg.pal.lightSleep).r);
  check("unknown is not awake",sleepState(2)==SLEEP_UNKNOWN);
  check("UTC fractions",parseUtc("2026-09-07T10:00:00.123Z")==parseUtc("2026-09-07T10:00:00+00:00"));
  check("invalid calendar rejected",parseUtc("2026-02-30T10:00:00Z")==0);
  check("leap day accepted",parseUtc("2024-02-29T10:00:00Z")>0);
  check("missing/invalid UTC rejected",!parseUtc(nullptr)&&!parseUtc("bad")&&!parseUtc("2026-09-07T10:00:00+02:00"));
  reset();poll(properties(130,80,8,0,"2023-01-01T00:00:00Z"));check("old first cloud value rejected",!vitalsFresh());
  reset();fresh();check("fresh unchanged new measurement accepted",vitalsFresh());
  sample(60,98);check("constant values with updated timestamps remain fresh",vitalsFresh());
  gSt.v.measuredAt=(uint32_t)fakeEpoch+1;check("future measurement rejected",!vitalsFresh());
  gSt.v.measuredAt=0;check("missing timestamp rejected",!vitalsFresh());
  reset();fresh();fakeEpoch+=61;gSt.lastOkAt=fakeNow;check("fetching cached measurement does not renew age",!vitalsFresh());
  reset();fresh();fakeNow+=20001;check("fetch outage expires readings",!cloudFresh()&&!vitalsFresh());
  reset();fresh();gSt.wifiOk=false;check("WiFi loss expires immediately",!vitalsFresh());
  reset();fresh();gSt.v.baseOn=false;check("base off gates readings",!vitalsFresh());gSt.v.baseOn=true;gSt.v.sockConn=0;check("disconnected gates readings",!vitalsFresh());
  reset();gCfg.ownAlarms=true;poll(properties(130,80,8,1));fakeEpoch+=10;fakeNow+=10000;poll(properties(130,80));
  check("charging session blocks cached values and own alarms",!vitalsFresh()&&!gSt.alSpo2);
  reset();gCfg.ownAlarms=true;fresh(80);sample(10);sample(15);check("own alarm waits full duration",!gSt.alSpo2);sample(20);check("own alarm after 15 measured seconds",gSt.alSpo2);
  reset();gCfg.ownAlarms=true;fresh(80);fakeNow+=15000;fakeEpoch+=15;alarmsEvaluate(true);check("duplicate measurement cannot advance duration",!gSt.alSpo2);
  gSt.cloudOk=false;alarmsEvaluate(false);check("failed fetch resets own duration",!gSt.spo2Since&&!gSt.alSpo2);
  gSt.cloudOk=true;sample(25);check("recovery starts duration over",!gSt.alSpo2);
  sample(90);check("measurement gap resets duration",!gSt.alSpo2);
  reset();poll(properties(130,98,8,0,"2026-09-07T10:00:00Z","CRIT_OX_ALRT","true"));check("critical oxygen flag reaches alarm",gAlarmCritical&&gSt.v.criticalOx);
  alarmAcknowledge();fakeNow+=40;dispTick();check("hush keeps critical display and brightness",gSt.screen==SCR_ALARM&&gSt.brightness==gCfg.briAlarm);check("hush quiets current cause",!anyAlarm());
  gSt.v.highHr=true;alarmRecompute();check("new cause re-arms sound",anyAlarm()&&gSt.soundPending&&!gSt.silenced);
  alarmAcknowledge();gSt.v.highHr=false;alarmRecompute();gSt.v.highHr=true;alarmRecompute();check("ended then repeated cause alarms anew",anyAlarm());
  reset();dispTest(TEST_VITALS,30);dispMessage("IP",30,0xffffff);gSt.v.lowOx=true;alarmRecompute();fakeNow+=40;dispTick();check("real alarm interrupts preview and message",gSt.screen==SCR_ALARM&&!gSt.testMode&&!gMsgActive);
  dispTest(TEST_INFO,30);check("active critical alarm rejects preview",gSt.testMode==TEST_OFF);
  reset();previewBegin();gCfg.pal.alarm=0;dispTest(TEST_VITALS,30);gSt.v.lowOx=true;alarmRecompute();fakeNow+=40;dispTick();check("alarm restores saved colours without waiting for web task",!gPrevOn&&gCfg.pal.alarm==Palette{}.alarm);
  reset();poll(properties(130,98,8,0,"2026-09-07T10:00:00Z","CRIT_BATT_ALRT","1"));check("critical battery represented as battery notice",gSt.v.criticalBatt&&gAlarmText.length()&&!gAlarmCritical);
  fakeNow+=21000;stateTick();dispTick();check("stale notice cannot hide waiting dashes",gSt.screen==SCR_WAITING);
  fakeNow+=9000;stateTick();dispTick();check("stale notice cannot hide offline",gSt.screen==SCR_OFFLINE);
  reset();gSt.v.criticalOx=true;alarmRecompute();fakeNow+=40;dispTick();check("last critical alarm retained during outage",gSt.screen==SCR_ALARM&&std::string(gAlarmText.c_str()).find("OFFLINE")!=std::string::npos);
  reset();gCfg.ownAlarms=true;gSt.alSpo2=true;gSt.spo2Since=1;alarmRecompute();config(R"({"ownAlarms":false})");check("disarm clears flags timers and display immediately",srv.status==200&&!gSt.alSpo2&&!gSt.spo2Since&&!gAlarmCritical);
  for(const char*body:{R"({"europe":false})",R"({"owletMail":"new@example.test"})",R"({"owletPass":"new"})",R"({"owletDsn":"serial-two"})"}){
    reset();readyNetwork();config(body);check("account/region/device change invalidates old session",srv.status==200&&!gSt.loggedIn&&gSt.authGeneration==1&&!gSt.dsn[0]&&owletTokenSecondsLeft()==0);
  }
  reset();readyNetwork();config(R"({"lang":"de"})");check("language save preserves cloud session",srv.status==200&&gSt.loggedIn&&gSt.authGeneration==0);
  for(const char*body:{R"({"spo2Seconds":-1})",R"({"spo2Seconds":0})",R"({"spo2Seconds":301})",R"({"spo2Seconds":"15"})",R"({"spo2Seconds":1.5})",R"({"ownAlarms":1})",R"({"briMin":256})",R"({"pollSeconds":1})",R"({"hrLowLimit":150,"hrHighLimit":100})",R"({"pal":{"heart":-1}})",R"({"pal":{"heart":16777216}})",R"({"lang":"xx"})",R"({"owletDsn":"../bad"})",R"({"webPass":"123456789012345678901234567890123"})","[]","invalid"}){
    reset();auto before=gCfg;config(body);check("invalid config rejected without mutation",srv.status==400&&memcmp(&gCfg,&before,sizeof(Config))==0);
  }
  reset();previewBegin();gCfg.pal.heart=0xffffff;config(R"({"lang":"de"})");check("saving unrelated field does not persist preview palette",gCfg.pal.heart==Palette{}.heart&&!gPrevOn);
  reset();config(R"({"pal":{"heart":1193046},"pollSeconds":15})");check("valid partial config applied",srv.status==200&&gCfg.pal.heart==0x123456&&gCfg.pollSeconds==15);
  reset();handleOtaFinish();check("empty OTA cannot reboot",srv.status==400&&ESP.restarts==0);
  reset();strlcpy(gCfg.webPass,"fixture",sizeof(gCfg.webPass));srv.auth=false;upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE);upload(UPLOAD_FILE_END);handleOtaFinish();check("unauthorized OTA rejected without restart",srv.status==401&&ESP.restarts==0&&!Update.running);
  reset();srv.origin="https://unrelated.test";upload(UPLOAD_FILE_START);handleOtaFinish();check("cross-origin upload rejected",srv.status==403&&ESP.restarts==0);
  reset();upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE);upload(UPLOAD_FILE_ABORTED);handleOtaFinish();check("aborted OTA cannot reboot",srv.status==400&&ESP.restarts==0&&!Update.running);
  for(int failure=0;failure<3;++failure){reset();Update.beginOk=failure!=0;Update.writeOk=failure!=1;Update.endOk=failure!=2;upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE);upload(UPLOAD_FILE_END);handleOtaFinish();check("failed flash step cannot reboot",srv.status==400&&ESP.restarts==0&&!Update.running);}
  reset();upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_END);handleOtaFinish();check("zero-byte upload rejected",srv.status==400&&!ESP.restarts);
  reset();srv.origin="http://owlanzi.local";upload(UPLOAD_FILE_START);upload(UPLOAD_FILE_WRITE);upload(UPLOAD_FILE_END);handleOtaFinish();check("completed valid OTA restarts once",srv.status==200&&ESP.restarts==1);handleOtaFinish();check("OTA success cannot be reused",srv.status==400&&ESP.restarts==1);
  reset();gCfg.soundEnabled=true;uint32_t before=fakeNow;soundAlarm(true);check("tone call never delays display",fakeNow==before&&toneActive);fakeNow+=90;soundTick();check("tone sequence advances on tick",toneStep==1);for(int n=0;n<9;++n){fakeNow+=100;soundTick();}check("manual tone terminates",!toneActive);
  reset();gCfg.soundEnabled=true;gSt.v.lowOx=true;alarmRecompute();soundTick();check("first critical cause plays immediately",toneActive&&!gSt.soundPending);alarmAcknowledge();soundTick();check("ack stops existing tone",!toneActive);gSt.v.highHr=true;alarmRecompute();soundTick();check("new cause sounds immediately after ack",toneActive);
  reset();fakeNow=0xfffffff0;dispTest(TEST_VITALS,1);fakeNow+=40;dispTick();check("preview survives millis wrap",gSt.testMode==TEST_VITALS);fakeNow+=1000;dispTick();check("preview expires across wrap",!gSt.testMode);
  reset();fakeNow=0xfffffff0;readyNetwork();fakeNow+=5000;check("token lifetime survives millis wrap",owletTokenSecondsLeft()==3475);
  reset();readyNetwork();replies.push_back({204,""});replies.push_back({200,properties()});check("APP_ACTIVE accepts empty successful response",owletPoll()&&gSt.appActiveOk&&gSt.lastError[0]==0&&gSt.v.measuredAt>0);
  reset();readyNetwork();for(int i=0;i<5;++i){replies.push_back({500,"failure"});replies.push_back({200,properties()});check("activation error remains diagnosable",owletPoll()&&!gSt.appActiveOk&&gSt.lastError[0]);}check("repeated activation failure retries login",!gSt.loggedIn);
  reset();readyNetwork();replies.push_back({204,""});replies.push_back({200,properties(),[]{cloudInvalidate();}});check("in-flight old-account result rejected",!owletPoll()&&!gSt.cloudOk&&!gSt.v.valid);
  reset();readyNetwork();replies.push_back({200,R"([{"device":{"dsn":"one"}},{"device":{"dsn":"two"}}])"});check("multiple devices require selection",!owletFindDevice()&&std::string(gSt.devices)=="one, two");
  strlcpy(gCfg.owletDsn,"two",sizeof(gCfg.owletDsn));replies.push_back({200,R"([{"device":{"dsn":"one"}},{"device":{"dsn":"two"}}])"});check("explicit device chosen",owletFindDevice()&&std::string(gSt.dsn)=="two");
  reset();readyNetwork();replies.push_back({200,R"([{"device":{"dsn":"only"}}])"});check("single paired device automatically selected",owletFindDevice()&&std::string(gSt.dsn)=="only");
  reset();readyNetwork();replies.push_back({200,R"({"idToken":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"})"});replies.push_back({200,R"({"mini_token":"fixture-mini-token"})"});replies.push_back({200,R"({"access_token":"fixture-access","refresh_token":"fixture-refresh","expires_in":3600})"});check("complete login chain succeeds",owletLogin()&&gSt.loggedIn&&owletTokenSecondsLeft()==3480);
  reset();readyNetwork();replies.push_back({200,R"({"access_token":"fixture-new","expires_in":3600})",[]{cloudInvalidate();}});gRefreshTok="fixture-refresh";check("old-account refresh cannot commit",!owletRefresh()&&!gSt.loggedIn&&owletTokenSecondsLeft()==0);
  reset();std::thread writer([]{for(int i=0;i<2000;++i){StateGuard lock;gSt.v.lowOx=i%2;alarmRecompute();}});std::thread reader([]{for(int i=0;i<2000;++i){StateGuard lock;String text=gAlarmText;assert((bool)gAlarmText.length()==gAlarmCritical);}});writer.join();reader.join();check("shared state and alarm string remain consistent under contention",true);
  testOnlineUpdates();
  printf("TOTAL %d passed, %d failed\n",passed,failed);return failed?1:0;
}
