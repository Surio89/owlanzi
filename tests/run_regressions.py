from pathlib import Path
import re
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.pio' / 'tests'

def write(name, text):
    p = OUT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding='utf-8')

def function(src, signature):
    start = src.index(signature)
    at = src.index('{', start)
    # Only used on these known C++ functions; comments and strings in them
    # contain balanced braces.
    depth = 1
    end = at + 1
    while depth:
        if src[end] == '{': depth += 1
        if src[end] == '}': depth -= 1
        end += 1
    return src[start:end]

write('Arduino.h', r'''
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <type_traits>
#include <ctime>
#include <cassert>
inline time_t fakeEpoch=1788775200;
inline time_t fakeTime(time_t *out){if(out)*out=fakeEpoch;return fakeEpoch;}
inline tm* gmtime_r(const time_t* value,tm* out){return gmtime_s(out,value)==0?out:nullptr;}
#define time fakeTime
class String {
  std::string value;
public:
  String() = default;
  String(const char *v):value(v?v:""){}
  String(const std::string &v):value(v){}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
  String(T v):value(std::to_string(v)){}
  const char *c_str() const { return value.c_str(); }
  size_t length() const { return value.length(); }
  char operator[](size_t i) const { return value[i]; }
  String& operator+=(const String& v) { value+=v.value; return *this; }
  friend String operator+(String a,const String& b) { return a+=b; }
  friend bool operator==(const String&a,const String&b){return a.value==b.value;}
  String substring(size_t a,size_t b)const{return value.substr(a,b-a);}
  size_t write(uint8_t c){value+=char(c);return 1;}
  size_t write(const uint8_t*p,size_t n){value.append((const char*)p,n);return n;}
};
inline uint32_t fakeNow=1000;
inline uint32_t millis(){return fakeNow;}
inline void delay(uint32_t ms){fakeNow+=ms;}
inline size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);if(n){memcpy(d,s,std::min(n-1,l));d[std::min(n-1,l)]=0;}return l;}
template<class T> T constrain(T v,T lo,T hi){return std::max(lo,std::min(hi,v));}
inline long map(long x,long a,long b,long c,long d){return (x-a)*(d-c)/(b-a)+c;}
inline void pinMode(int,int){}
inline int analogRead(int){return 0;}
inline void ledcSetup(int,int,int){}
inline void ledcAttachPin(int,int){}
inline void ledcWrite(int,int){}
inline void ledcWriteTone(int,int){}
inline uint8_t pgm_read_byte(const uint8_t*p){return *p;}
inline struct SerialStub{ template<class... T>void printf(const char*,T...){} void println(const char*){} } Serial;
inline struct ESPStub {int restarts=0;uint32_t flashSize=4194304;void restart(){++restarts;}uint32_t getFlashChipSize(){return flashSize;}} ESP;
#define OUTPUT 1
#define PROGMEM
#define __attribute__(x)
''')
write('Preferences.h', r'''
#pragma once
inline int preferenceWrites=0;
inline uint32_t savedDailyDay=0;
inline bool dailyReadOk=true,dailyWriteOk=true;
class Preferences {public:
bool begin(const char* name,bool readOnly){return std::string(name)!="owlanzi-update" || !readOnly || dailyReadOk;} void end(){} void clear(){}
uint32_t getUInt(const char* key,uint32_t d){assert(std::string(key)=="day");return savedDailyDay?savedDailyDay:d;}
size_t putUInt(const char* key,uint32_t value){assert(std::string(key)=="day");if(!dailyWriteOk)return 0;savedDailyDay=value;++preferenceWrites;return sizeof(value);}
int getInt(const char*,int d){return d;} bool getBool(const char*,bool d){return d;}
void getString(const char*,char*,size_t){} size_t getBytesLength(const char*){return 0;}
void getBytes(const char*,void*,size_t){} void putInt(const char*,int){++preferenceWrites;}
void putBool(const char*,bool){++preferenceWrites;} void putString(const char*,const char*){++preferenceWrites;}
void putBytes(const char*,void*,size_t){++preferenceWrites;}
};
''')
write('freertos/FreeRTOS.h', '#pragma once\n#define portMAX_DELAY 0xffffffff\n#define configASSERT assert\n')
write('freertos/task.h', '#pragma once\ninline uint32_t uxTaskGetStackHighWaterMark(void*){return 12000;}\n')
write('freertos/semphr.h', r'''
#pragma once
#include <mutex>
using SemaphoreHandle_t = std::recursive_mutex*;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(){return new std::recursive_mutex;}
inline void xSemaphoreTakeRecursive(SemaphoreHandle_t m,unsigned){m->lock();}
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t m){m->unlock();}
''')
write('FastLED.h', r'''
#pragma once
#include "Arduino.h"
struct CRGB {uint8_t r=0,g=0,b=0;CRGB()=default;CRGB(int a,int d,int c):r(a),g(d),b(c){}
static const CRGB Black,Red,Green,Blue,White;};
inline const CRGB CRGB::Black{0,0,0},CRGB::Red{255,0,0},CRGB::Green{0,255,0},CRGB::Blue{0,0,255},CRGB::White{255,255,255};
inline void fill_solid(CRGB*p,size_t n,CRGB c){std::fill(p,p+n,c);}
struct WS2812B{};
inline constexpr int GRB=0,TypicalLEDStrip=0;
inline struct LedStub{template<class T,int P,int O>void addLeds(CRGB*,int){} void setCorrection(int){} void setBrightness(int){} void show(){}}FastLED;
''')

write('WiFiClientSecure.h', '''#pragma once
class WiFiClient {public:
 std::string bytes;size_t pos=0;bool stalled=false;int chunk=1024;
 int available(){return stalled?0:(int)std::min(bytes.size()-pos,(size_t)chunk);}
 int read(uint8_t*out,size_t size){size=std::min(size,bytes.size()-pos);memcpy(out,bytes.data()+pos,size);pos+=size;return (int)size;}
 bool connected(){return stalled || pos<bytes.size();}
};
inline bool certificateConfigured=false;
inline int secureTimeoutSeconds=0,handshakeTimeoutSeconds=0,fakeTlsError=0;
struct WiFiClientSecure {
 void setCACertBundle(const uint8_t*){certificateConfigured=true;}
 void setTimeout(int seconds){secureTimeoutSeconds=seconds;}
 void setHandshakeTimeout(int seconds){handshakeTimeoutSeconds=seconds;}
 int lastError(char*,size_t){return fakeTlsError;}
};
''')
write('HTTPClient.h', r'''
#pragma once
#include <deque>
#include <sstream>
#include <functional>
struct Reply {int code;std::string body;std::function<void()> hook={};int length=-2;bool stall=false;int chunk=1024;bool noStream=false;};
inline std::deque<Reply> replies;
inline std::vector<std::string> requestedUrls;
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient { Reply reply; std::istringstream stream;WiFiClient raw;bool http10=false;
public:
void setTimeout(int){} void setReuse(bool){}
void setConnectTimeout(int){}void useHTTP10(bool value){http10=value;}void setFollowRedirects(int value){assert(value==0);}
bool begin(WiFiClientSecure&,const String&url){requestedUrls.push_back(url.c_str());return true;}
void addHeader(const char*,const String&){}
int GET(){assert(!replies.empty());reply=replies.front();replies.pop_front();if(reply.hook)reply.hook();raw.bytes=reply.body;raw.stalled=reply.stall;raw.chunk=reply.chunk;return reply.code;}
int POST(uint8_t*,size_t){return GET();}
String getString(){return reply.body;} int getSize(){return reply.length!=-2?reply.length:http10?(int)reply.body.size():-1;}
WiFiClient* getStreamPtr(){return reply.noStream?nullptr:&raw;}
std::istringstream& getStream(){stream.str(reply.body);return stream;}
String errorToString(int){return "simulated transport failure";} void end(){}
};
''')
write('Update.h', '#pragma once\n')
write('esp_ota_ops.h', r'''
#pragma once
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_TYPE_APP=0;
constexpr int ESP_PARTITION_SUBTYPE_DATA_NVS=2,ESP_PARTITION_SUBTYPE_DATA_OTA=0;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0=16,ESP_PARTITION_SUBTYPE_APP_OTA_1=17;
struct esp_partition_t {uint32_t address,size;};
inline esp_partition_t nvsPart{0x9000,0x5000},dataPart{0xe000,0x2000},app0Part{0x10000,0x1e0000},app1Part{0x1f0000,0x1e0000};
inline const esp_partition_t* esp_partition_find_first(int type,int subtype,const char*){
 if(type==ESP_PARTITION_TYPE_APP)return subtype==16?&app0Part:&app1Part;
 return subtype==2?&nvsPart:&dataPart;
}
inline const esp_partition_t* esp_ota_get_running_partition(){return &app0Part;}
inline const esp_partition_t* esp_ota_get_next_update_partition(void*){return &app1Part;}
''')
write('mbedtls/sha256.h',r'''
#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
struct mbedtls_sha256_context {BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE handle=nullptr;std::vector<uint8_t> object;};
inline void mbedtls_sha256_init(mbedtls_sha256_context*){}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context*c,int){
 if(BCryptOpenAlgorithmProvider(&c->alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return -1;
 ULONG size=0,got=0;if(BCryptGetProperty(c->alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0)<0)return -1;
 c->object.resize(size);return BCryptCreateHash(c->alg,&c->handle,c->object.data(),size,nullptr,0,0)<0?-1:0;
}
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context*c,const uint8_t*p,size_t n){return BCryptHashData(c->handle,(PUCHAR)p,(ULONG)n,0)<0?-1:0;}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context*c,uint8_t*out){return BCryptFinishHash(c->handle,out,32,0)<0?-1:0;}
inline void mbedtls_sha256_free(mbedtls_sha256_context*c){if(c->handle)BCryptDestroyHash(c->handle);if(c->alg)BCryptCloseAlgorithmProvider(c->alg,0);}
''')
web=(ROOT/'src/webui.cpp').read_text(encoding='utf-8')
assert not any(marker in web for marker in ('\u00c3', '\u00c2', '\u00e2\u20ac', '\ufffd')), 'Web UI contains incorrectly decoded UTF-8'
parts=[r'''
#include "Arduino.h"
#include <vector>
#include <thread>
#include <ArduinoJson.h>
namespace ArduinoJson {
template<> struct Converter<String> {
static String fromJson(JsonVariantConst src){return String(src.as<std::string>());}
static void toJson(const String&src,JsonVariant dst){dst.set(src.c_str());}
};
}
template<class... Args> DeserializationError deserializeJson(JsonDocument&d,const String&s,Args...args){return deserializeJson(d,s.c_str(),args...);}
template<class... Args> DeserializationError deserializeJson(JsonDocument&d,String&s,Args...args){return deserializeJson(d,s.c_str(),args...);}
template<class... Args> DeserializationError deserializeJson(JsonDocument&d,String&&s,Args...args){return deserializeJson(d,s.c_str(),args...);}
#include "../../src/config.cpp"
#include "../../src/display.cpp"
#include "../../src/vitals.cpp"
#define asm(x)
#include "../../src/owlet.cpp"
#undef asm
extern const uint8_t rootca_crt_bundle_start[]={0},rootca_crt_bundle_end[]={0};
enum {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
constexpr int UPDATE_SIZE_UNKNOWN=-1;
constexpr int U_FLASH=0;
struct HTTPUpload {int status=UPLOAD_FILE_START;uint8_t *buf=nullptr;size_t currentSize=0;};
struct ServerStub {
 bool auth=true;std::string body,origin;int status=0;HTTPUpload up;
 String arg(const char*){return body;} bool authenticate(const char*,const char*){return auth;}
 bool hasHeader(const char*){return !origin.empty();}String header(const char*){return origin;}
 String hostHeader(){return "owlanzi.local";}
 HTTPUpload& upload(){return up;}
 void requestAuthentication(){status=401;}
 void send(int s,const char*,const String&){status=s;}void sendHeader(const char*,const char*){}
}srv;
struct UpdateStub {
 bool running=false,beginOk=true,writeOk=true,endOk=true;int aborts=0,begins=0,ends=0;uint32_t expected=0;std::string written;
 bool begin(uint32_t size,int command=0){assert(command==U_FLASH);++begins;expected=size;written.clear();return running=beginOk;}bool isRunning(){return running;}
 size_t write(uint8_t*p,size_t n){if(writeOk&&p)written.append((char*)p,n);return writeOk?n:0;}
 bool end(bool incomplete){++ends;running=false;return endOk&&(incomplete||written.size()==expected);}
 void abort(){running=false;++aborts;}
}Update;
#define asm(x)
#include "../../src/online_update.cpp"
#undef asm
''',function(web,'static bool authorized()'),function(web,'static bool sameOrigin()'),function(web,'static bool guard()'),
function(web,'static const char *whyEn()'),function(web,'static const char *whyDe()'),
function(web,'static void palFromJson('),function(web,'static void handleConfigPost()'),
web[web.index('static bool otaStarted='):web.index('// The setup page.')],
(ROOT/'tests/regressions.cpp').read_text(encoding='utf-8'),
(ROOT/'tests/online_update_tests.cpp').read_text(encoding='utf-8')]
write('regressions.cpp','\n'.join(parts))
write('run.cmd',r'''@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /std:c++17 /EHsc /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS /I. /I"..\libdeps\release\ArduinoJson\src" regressions.cpp /Fe:regressions.exe
if errorlevel 1 exit /b 1
regressions.exe
''')
strings={m[1]:m[3] for m in re.finditer(r'static const char (\w+)\[\] PROGMEM = R"(\w+)\((.*?)\)\2";',web,re.S)}
for name,fragments in {'main':['P_HEAD','CSS','PW_JS','P_A','LOGO','P_B','P_C','P_D','P_JS1','P_JS2','P_JS3'],'setup':['SETUP_PAGE','CSS','PW_JS','LOGO','SETUP_BODY']}.items():
    html=''.join(strings[n] for n in fragments)
    write(name+'.html',html)
    write(name+'.js','\n'.join(re.findall(r'<script>(.*?)</script>',html,re.S)))
    subprocess.run(['node','--check',str(OUT/(name+'.js'))],check=True)
subprocess.run(['cmd','/c',str(OUT/'run.cmd')],check=True)
subprocess.run(['node','--test',str(ROOT/'tests/browser.test.mjs')],check=True)
subprocess.run([os.sys.executable,str(ROOT/'tests/check_update_stack.py')],check=True)
