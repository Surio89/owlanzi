#include "online_update.h"
#include "display.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <new>
#include <Preferences.h>

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
static constexpr uint32_t SLOT_SIZE=0x1e0000;
static constexpr uint32_t IDLE_TIMEOUT=15000, TOTAL_TIMEOUT=180000;
static const char *ORIGIN="https://owlanzi.com/firmware/";
static UpdateOwner owner=UpdateOwner::NONE;
static bool queued=false,installRequested=false;
static bool dailyRequested=false,dailyLoaded=false;
static uint32_t lastDailyDay=0;
static OnlineRelease latest;
static const char *phase="idle",*errorCode="";
static uint32_t received=0;
static int lastHttpStatus=0,lastTlsError=0;
static uint32_t stackFreeMin=0;

const char *updateTarget() {
#ifdef ULANZI_TC001
  return "tc001";
#else
  return "esp32dev";
#endif
}
static bool versionParts(const char *s,uint32_t out[3]) {
  if(!s || !*s)return false;
  for(int i=0;i<3;++i) {
    if(!isdigit((unsigned char)*s))return false;
    uint32_t n=0;int digits=0;
    while(isdigit((unsigned char)*s)) {if(++digits>5)return false;n=n*10+(*s++-'0');}
    out[i]=n;
    if(i<2 && *s++!='.')return false;
  }
  return *s==0;
}
bool newerVersion(const char *candidate,const char *current) {
  uint32_t a[3],b[3];
  if(!versionParts(candidate,a) || !versionParts(current,b))return false;
  for(int i=0;i<3;++i)if(a[i]!=b[i])return a[i]>b[i];
  return false;
}
bool parseOnlineRelease(JsonDocument &d,OnlineRelease &r) {
  if(!d.is<JsonObject>() || (d["schema"] | 0)!=1 ||
     strcmp(d["target"] | "",updateTarget()) || strcmp(d["chip"] | "","ESP32") ||
     strcmp(d["kind"] | "","ota-app") || strcmp(d["layout"] | "","owlanzi-4m-v1") ||
     (d["flash_size_bytes"] | 0)!=4194304 || !d["size_bytes"].is<uint32_t>())return false;
  const char *v=d["version"] | "",*file=d["file"] | "",*hash=d["sha256"] | "";
  uint32_t parts[3];
  if(strlen(v)>=sizeof(r.version) || !versionParts(v,parts) || strlen(hash)!=64)return false;
  for(const char *p=hash;*p;++p)if(!isxdigit((unsigned char)*p))return false;
  char expected[80];snprintf(expected,sizeof(expected),"owlanzi-%s-%s-ota.bin",updateTarget(),v);
  if(strcmp(file,expected))return false; // fixed HTTPS origin; no redirects or arbitrary URLs
  uint32_t size=d["size_bytes"].as<uint32_t>();
  if(size<1024 || size>SLOT_SIZE)return false;
  strlcpy(r.version,v,sizeof(r.version));strlcpy(r.file,file,sizeof(r.file));
  for(size_t i=0;i<64;++i)r.sha256[i]=(char)tolower((unsigned char)hash[i]);
  r.sha256[64]=0;r.size=size;
  return true;
}
bool updateClaim(UpdateOwner who) {
  StateGuard lock;
  if(owner!=UpdateOwner::NONE)return false;
  owner=who;return true;
}
void updateRelease(UpdateOwner who) {StateGuard lock;if(owner==who)owner=UpdateOwner::NONE;}
bool updateOwnedBy(UpdateOwner who) {StateGuard lock;return owner==who;}
bool updateBusy() {StateGuard lock;return owner!=UpdateOwner::NONE;}
static void status(const char *p,const char *error="") {StateGuard lock;phase=p;errorCode=error;}
static bool stopRequired() {StateGuard lock;return !gSt.wifiOk || gAlarmCritical;}
static bool requestUpdate(bool install,bool daily) {
  StateGuard lock;
  if(updateBusy())return false;
  if(!gSt.wifiOk || gSt.apMode){status("error","offline");return false;}
  if(time(nullptr)<1700000000){status("error","clock");return false;}
  if(gAlarmCritical){status("error","alarm");return false;}
  updateClaim(UpdateOwner::ONLINE);queued=true;installRequested=install;dailyRequested=daily;
  // Keep the last successfully checked version visible during retries/errors.
  received=0;lastHttpStatus=lastTlsError=0;status("queued");return true;
}
bool onlineUpdateRequest(bool install) {return requestUpdate(install,false);}

// Same calendar day as the aggregate dashboard (Europe/Berlin), independent
// of the device's display settings. EU DST changes at 01:00 UTC on the last
// Sunday in March/October. Never change the process-wide timezone for this.
static uint32_t dailyCheckDay(time_t now) {
  if(now<1700000000)return 0;
  struct tm utc;
  if(!gmtime_r(&now,&utc))return 0;
  bool summer=utc.tm_mon>2 && utc.tm_mon<9;
  if(utc.tm_mon==2 || utc.tm_mon==9) {
    int sunday=31-(utc.tm_wday+31-utc.tm_mday)%7;
    bool after=utc.tm_mday>sunday || (utc.tm_mday==sunday && utc.tm_hour>=1);
    summer=utc.tm_mon==2 ? after : !after;
  }
  return (uint32_t)(((int64_t)now+(summer?7200:3600))/86400);
}
static void scheduleDailyCheck() {
  {StateGuard lock;
    if(queued || updateBusy() || !gCfg.autoUpdateCheck || !gSt.wifiOk || gSt.apMode || gAlarmCritical)return;
  }
  // Let Wi-Fi, NTP and the initial cloud reading settle first.
  if(millis()<60000)return;
  uint32_t day=dailyCheckDay(time(nullptr));
  if(!day)return;
  if(!dailyLoaded) {
    Preferences daily;
    if(daily.begin("owlanzi-update",true)) {
      lastDailyDay=daily.getUInt("day",0);daily.end();
    }
    dailyLoaded=true;
  }
  // A clock moving backwards must not cause another count for an earlier day.
  if(day>lastDailyDay)requestUpdate(false,true);
}
static bool reserveDailyCheck(uint32_t day) {
  // Reserve BEFORE the request: a lost response or a reboot must not cause a
  // duplicate marker. Failed automatic attempts wait for the next day; manual
  // checks still work. Only this local calendar day is stored, never an ID.
  if(!day || day<=lastDailyDay)return false;
  lastDailyDay=day;
  Preferences daily;
  if(!daily.begin("owlanzi-update",false))return false;
  uint32_t savedDay=daily.getUInt("day",0);
  if(savedDay>=day) {lastDailyDay=savedDay;daily.end();return false;}
  bool saved=daily.putUInt("day",day)==sizeof(day);
  daily.end();return saved;
}
void onlineUpdateJson(JsonObject d) {
  StateGuard lock;
  d["current"]=OWLANZI_VERSION;d["latest"]=latest.version;
  d["phase"]=phase;d["error"]=errorCode;d["busy"]=updateBusy();
  d["available"]=newerVersion(latest.version,OWLANZI_VERSION);
  d["received"]=received;d["total"]=latest.size;
  d["http_status"]=lastHttpStatus;d["tls_error"]=lastTlsError;
  d["stack_free_min"]=stackFreeMin;
}
static bool compatibleLayout() {
  const esp_partition_t *nvs=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"nvs");
  const esp_partition_t *a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr);
  const esp_partition_t *b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr);
  const esp_partition_t *data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,nullptr);
  const esp_partition_t *running=esp_ota_get_running_partition(),*next=esp_ota_get_next_update_partition(nullptr);
  return ESP.getFlashChipSize()==4194304 && nvs && nvs->address==0x9000 && nvs->size==0x5000 &&
    data && data->address==0xe000 && data->size==0x2000 && a && a->address==0x10000 && a->size==SLOT_SIZE &&
    b && b->address==0x1f0000 && b->size==SLOT_SIZE && running && next &&
    (running->address==a->address || running->address==b->address) && next->address!=running->address &&
    (next->address==a->address || next->address==b->address);
}
static int openDownload(HTTPClient &http,WiFiClientSecure &client,const String &url) {
  client.setCACertBundle(rootca_crt_bundle_start);
  // Arduino-ESP32 2.x secure-client timeouts are seconds; HTTPClient uses ms.
  client.setTimeout(IDLE_TIMEOUT/1000);
  client.setHandshakeTimeout(IDLE_TIMEOUT/1000);
  http.setTimeout(IDLE_TIMEOUT);http.setConnectTimeout(IDLE_TIMEOUT);
  http.setReuse(false);http.useHTTP10(true);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if(!http.begin(client,url))return -1;
  http.addHeader("Accept-Encoding","identity");http.addHeader("Cache-Control","no-cache");
  int code=http.GET();
  char tlsMessage[128]={0};int rawTlsResult=client.lastError(tlsMessage,sizeof(tlsMessage));
  // Arduino 2.x stores start_ssl_client's nonnegative success return too.
  // Only negative values accompanying a transport failure are TLS errors.
  int tlsError=code<0 && rawTlsResult<0 ? rawTlsResult : 0;
  uint32_t stackMin=uxTaskGetStackHighWaterMark(nullptr);
  {StateGuard lock;lastHttpStatus=code;lastTlsError=tlsError;stackFreeMin=stackMin;}
  Serial.printf("[update] HTTP %d, TLS %d, stack minimum %u bytes\n",code,tlsError,(unsigned)stackMin);
  return code;
}
static bool readExact(WiFiClient &stream,uint8_t *dest,size_t size,uint32_t started) {
  size_t at=0;uint32_t last=millis();
  while(at<size) {
    if(stopRequired()){status("error","interrupted");return false;}
    if(millis()-last>=IDLE_TIMEOUT || millis()-started>=TOTAL_TIMEOUT){status("error","timeout");return false;}
    int available=stream.available();
    if(available>0) {
      size_t amount=std::min(size-at,(size_t)available);
      int n=stream.read(dest+at,amount);
      if(n>0){at+=(size_t)n;last=millis();}
      else delay(1);
    } else if(!stream.connected()){status("error","incomplete");return false;}
    else delay(1);
  }
  return true;
}
static bool __attribute__((noinline)) fetchRelease(OnlineRelease &release,bool daily) {
  WiFiClientSecure client;HTTPClient http;
  String url=String(ORIGIN)+"ota-"+updateTarget()+".json";
  if(daily)url+="?daily-update-check=1";
  int code=openDownload(http,client,url);int size=http.getSize();
  bool ok=false;
  if(code!=200)status("error","network");
  else if(size<=0 || size>2048)status("error","manifest");
  else {
    // Allocate after TLS connects. A local 2 KB array stayed on the task stack
    // throughout the handshake, on top of the inlined install path in 1.0.4.
    std::unique_ptr<uint8_t[]> json(new(std::nothrow) uint8_t[size+1]);
    WiFiClient *stream=http.getStreamPtr();
    if(!json)status("error","memory");
    else if(!stream)status("error","incomplete");
    else if(readExact(*stream,json.get(),(size_t)size,millis())) {
      json[size]=0;JsonDocument d;
      if(deserializeJson(d,(char*)json.get()) || !parseOnlineRelease(d,release))status("error","manifest");
      else ok=true;
    }
  }
  http.end();return ok;
}
static bool __attribute__((noinline)) installRelease(const OnlineRelease &release) {
  if(!compatibleLayout()){status("error","layout");return false;}
  WiFiClientSecure client;HTTPClient http;
  int code=openDownload(http,client,String(ORIGIN)+release.file);
  if(code!=200){status("error","network");http.end();return false;}
  if(http.getSize()!=(int)release.size){status("error","size");http.end();return false;}
  constexpr size_t BUFFER_SIZE=1024;
  std::unique_ptr<uint8_t[]> buffer(new(std::nothrow) uint8_t[BUFFER_SIZE]);
  WiFiClient *stream=http.getStreamPtr();
  if(!buffer){status("error","memory");http.end();return false;}
  if(!stream){status("error","incomplete");http.end();return false;}
  uint32_t started=millis();
  if(!readExact(*stream,buffer.get(),24,started)){http.end();return false;}
  // USB merged files and images for another flash geometry are never written.
  if(buffer[0]!=0xe9 || buffer[2]!=2 || buffer[3]!=0x20 || buffer[12]!=0 || buffer[13]!=0){
    status("error","image");http.end();return false;
  }
  if(!Update.begin(release.size,U_FLASH)){status("error","flash");http.end();return false;}
  mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);
  bool ok=mbedtls_sha256_starts_ret(&hash,0)==0;
  if(!ok)status("error","hash");
  size_t count=24;uint32_t done=0;
  if(ok)status("downloading");
  while(ok) {
    ok=mbedtls_sha256_update_ret(&hash,buffer.get(),count)==0;
    if(!ok){status("error","hash");break;}
    if(Update.write(buffer.get(),count)!=count){status("error","flash");ok=false;break;}
    done+=(uint32_t)count;
    {StateGuard lock;received=done;}
    if(done==release.size)break;
    count=std::min(BUFFER_SIZE,(size_t)(release.size-done));
    ok=readExact(*stream,buffer.get(),count,started);
    delay(1); // allow Wi-Fi/RTOS work while flash writes stream in
  }
  uint8_t digest[32];char hex[65];
  if(ok) {
    status("verifying");ok=mbedtls_sha256_finish_ret(&hash,digest)==0;
    for(int i=0;i<32 && ok;++i)snprintf(hex+i*2,3,"%02x",digest[i]);
    if(!ok || strcmp(hex,release.sha256)){status("error","hash");ok=false;}
  }
  mbedtls_sha256_free(&hash);http.end();
  if(ok && stopRequired()){status("error","interrupted");ok=false;}
  // end() selects the next boot partition. Do this only AFTER SHA-256 matches.
  if(ok && !Update.end(false)){status("error","flash");ok=false;}
  if(!ok)Update.abort();
  return ok;
}
bool onlineUpdateTick() {
  scheduleDailyCheck();
  bool install,daily;
  {StateGuard lock;if(!queued)return false;queued=false;install=installRequested;daily=dailyRequested;status("checking");}
  if(stopRequired()){status("error","interrupted");updateRelease(UpdateOwner::ONLINE);return true;}
  // If NVS cannot persist the day, still check for updates without the marker.
  if(daily)daily=reserveDailyCheck(dailyCheckDay(time(nullptr)));
  OnlineRelease release;
  bool ok=fetchRelease(release,daily);
  if(ok) {
    {StateGuard lock;latest=release;}
    bool newer=newerVersion(release.version,OWLANZI_VERSION);
    dispUpdateAvailable(newer?release.version:"");
    if(!newer)status("current");
    else if(!install)status("available");
    else if(installRelease(release)) {
      status("restarting");delay(1500);ESP.restart();
      return true; // keep ownership until reboot; no second flash writer
    }
  }
  updateRelease(UpdateOwner::ONLINE);return true;
}
