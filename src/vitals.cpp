#include "vitals.h"

// Calendar conversion is independent of the device's timezone and works in
// host tests as well as the ESP32. Reject normalized/invalid calendar dates.
uint32_t parseUtc(const char *s) {
  if (!s) return 0;
  int y,m,d,h,mi,se,n=0;
  if (sscanf(s,"%d-%d-%dT%d:%d:%d%n",&y,&m,&d,&h,&mi,&se,&n)!=6) return 0;
  const char *tail=s+n;
  if (*tail=='.') { ++tail; if(!isdigit((unsigned char)*tail))return 0; while(isdigit((unsigned char)*tail))++tail; }
  if (strcmp(tail,"Z") && strcmp(tail,"+00:00")) return 0;
  if(y<2023 || y>2100 || m<1 || m>12 || d<1 || h<0 || h>23 || mi<0 || mi>59 || se<0 || se>59)return 0;
  const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap=(y%4==0 && (y%100!=0 || y%400==0));
  if(d>days[m-1]+(m==2 && leap))return 0;
  int64_t total=0;
  for(int year=1970;year<y;++year)total+=365+(year%4==0 && (year%100!=0 || year%400==0));
  for(int month=1;month<m;++month)total+=days[month-1]+(month==2 && leap);
  total=(total+d-1)*86400+h*3600+mi*60+se;
  return (uint32_t)total;
}
static bool boolProp(JsonVariant v) {
  if(v.is<bool>())return v.as<bool>();
  if(v.is<const char*>()) { const char *s=v.as<const char*>(); return !strcmp(s,"true") || !strcmp(s,"1"); }
  return v.as<int>()!=0;
}
bool parseVitals(JsonDocument &res,Vitals &v,char *error,size_t size) {
  auto fail=[&](const char *s){strlcpy(error,s,size);return false;};
  JsonArray arr=res.as<JsonArray>();
  if(arr.isNull())return fail("properties: not an array");
  v=Vitals{};
  String raw;
  for(JsonObject o:arr) {
    const char *n=o["property"]["name"];
    if(!n)continue;
    JsonVariant val=o["property"]["value"];
    if(!strcmp(n,"REAL_TIME_VITALS")) {
      raw=val.as<String>();
      v.measuredAt=parseUtc(o["property"]["data_updated_at"] | "");
    }
    else if(!strcmp(n,"LOW_OX_ALRT"))v.lowOx=boolProp(val);
    else if(!strcmp(n,"HIGH_OX_ALRT"))v.highOx=boolProp(val);
    else if(!strcmp(n,"LOW_HR_ALRT"))v.lowHr=boolProp(val);
    else if(!strcmp(n,"HIGH_HR_ALRT"))v.highHr=boolProp(val);
    else if(!strcmp(n,"CRIT_OX_ALRT"))v.criticalOx=boolProp(val);
    else if(!strcmp(n,"CRIT_BATT_ALRT"))v.criticalBatt=boolProp(val);
    else if(!strcmp(n,"LOST_POWER_ALRT"))v.lostPower=boolProp(val);
    else if(!strcmp(n,"SOCK_DISCON_ALRT"))v.sockDiscon=boolProp(val);
    else if(!strcmp(n,"SOCK_OFF"))v.sockOff=boolProp(val);
    else if(!strcmp(n,"LOW_BATT_ALRT"))v.lowBatt=boolProp(val);
  }
  JsonDocument j;
  if(!raw.length() || deserializeJson(j,raw) || !j.is<JsonObject>())return fail("REAL_TIME_VITALS missing or invalid");
  v.oxygen=j["ox"] | 0.0f; v.heart=j["hr"] | 0.0f;
  v.battery=j["bat"] | 0.0f; v.oxygen10=j["oxta"] | 0.0f;
  v.baseOn=boolProp(j["bso"]); v.sleepSt=j["ss"] | 0;
  v.charging=j["chg"] | 0; v.sockConn=j["sc"] | 0; v.movement=j["mv"] | 0;
  strlcpy(v.hardware,j["hw"] | "",sizeof(v.hardware));
  if(!isfinite(v.heart) || v.heart<0 || v.heart>400 || !isfinite(v.oxygen) || v.oxygen<0 || v.oxygen>100 ||
     !isfinite(v.battery) || v.battery<0 || v.battery>100)return fail("Vitals out of range");
  v.valid=true; v.fetchedAt=millis(); error[0]=0;
  return true;
}
void acceptVitals(const Vitals &v,bool appActive) {
  StateGuard lock;
  bool inactive=v.charging || v.sockOff || !v.baseOn || !v.sockConn;
  if(inactive) { gSt.sessionPending=true; alarmsResetOwn(); }
  else if(gSt.sessionPending) {
    time_t now=time(nullptr);
    if(now>=1700000000) { gSt.sessionAfter=(uint32_t)now+5; gSt.sessionPending=false; }
  }
  gSt.v=v; gSt.cloudOk=true; gSt.appActiveOk=appActive;
  gSt.lastOkAt=millis();
  alarmsEvaluate(true);
}
