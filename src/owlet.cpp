/*
 * owlet.cpp - the login chain and the data fetch
 *
 *   1. Firebase   mail + password        -> idToken (JWT, ~940 chars)
 *   2. Owlet SSO  idToken                -> mini_token
 *   3. Ayla       mini_token + secret    -> access_token (valid 24 h)
 *   4. Ayla       access_token           -> device list (dsn)
 *   5. Ayla       APP_ACTIVE = 1         (without it the cloud freezes)
 *   6. Ayla       properties.json        -> REAL_TIME_VITALS
 *
 * Endpoints and field names taken from:
 *   github.com/ryanbdclark/pyowletapi (src/pyowletapi/const.py)
 *   github.com/mbevand/owlet_monitor
 */
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "owlet.h"
#include "vitals.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

static const char *EU_KEY   = "AIzaSyDm6EhV70wudwN3iOSq3vTjtsdGjdFLuuM";
static const char *EU_ID    = "OwletCare-Android-EU-fw-id";
static const char *EU_SEC   = "OwletCare-Android-EU-JKupMPBoj_Npce_9a95Pc8Qo0Mw";
static const char *EU_MINI  = "https://ayla-sso.eu.owletdata.com/mini/";
static const char *EU_SIGN  = "https://user-field-eu-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *EU_REFR  = "https://user-field-eu-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *EU_BASE  = "https://ads-field-eu-1a2039d9.aylanetworks.com/apiv1";

static const char *WO_KEY   = "AIzaSyCsDZ8kWxQuLJAMVnmEhEkayH1TSxKXfGA";
static const char *WO_ID    = "sso-prod-3g-id";
static const char *WO_SEC   = "sso-prod-UEjtnPCtFfjdwIwxqnC0OipxRFU";
static const char *WO_MINI  = "https://ayla-sso.owletdata.com/mini/";
static const char *WO_SIGN  = "https://user-field-1a2039d9.aylanetworks.com/api/v1/token_sign_in";
static const char *WO_REFR  = "https://user-field-1a2039d9.aylanetworks.com/users/refresh_token.json";
static const char *WO_BASE  = "https://ads-field-1a2039d9.aylanetworks.com/apiv1";

static Config netCfg;
static uint32_t netGeneration=0, tokenGeneration=0;
static void beginRequest() { StateGuard lock; netCfg=gCfg; netGeneration=gSt.authGeneration; }
static bool currentRequest() { StateGuard lock; return netGeneration==gSt.authGeneration; }

#define RK (netCfg.europe ? EU_KEY  : WO_KEY)
#define RI (netCfg.europe ? EU_ID   : WO_ID)
#define RS (netCfg.europe ? EU_SEC  : WO_SEC)
#define RM (netCfg.europe ? EU_MINI : WO_MINI)
#define RG (netCfg.europe ? EU_SIGN : WO_SIGN)
#define RR (netCfg.europe ? EU_REFR : WO_REFR)
#define RB (netCfg.europe ? EU_BASE : WO_BASE)

static const char *ANDROID_PKG  = "com.owletcare.owletcare";
static const char *ANDROID_CERT = "2A3BC26DB0B8B0792DBE28E6FFDC2598F9B12B74";

static String   gAccess, gRefreshTok;
static uint32_t gIssuedAt=0, gLifetime=0;

uint32_t owletTokenSecondsLeft() {
  StateGuard lock;
  if(tokenGeneration!=gSt.authGeneration)return 0;
  uint32_t age=(millis()-gIssuedAt)/1000;
  return age<gLifetime ? gLifetime-age : 0;
}
static void setTokenLifetime(uint32_t ttl) {
  gIssuedAt=millis(); ttl=constrain(ttl,1u,86400u);
  gLifetime=ttl>120 ? ttl-120 : ttl;
  tokenGeneration=netGeneration;
}
static void fail(const char *fmt, ...) {
  StateGuard lock;
  if(!currentRequest())return;
  va_list ap; va_start(ap, fmt);
  vsnprintf(gSt.lastError, sizeof(gSt.lastError), fmt, ap);
  va_end(ap);
  Serial.printf("   !! %s\n", gSt.lastError);
}

/*
 * One HTTPS call with filtered JSON parsing.
 *
 * IMPORTANT, this one cost an hour:
 * with "Transfer-Encoding: chunked", HTTPClient::getStream() hands the chunk
 * length markers through as part of the data. ArduinoJson trips over them
 * and returns an empty document WITHOUT reporting an error. That turned the
 * 940 character JWT into the string "null". Every Owlet response is chunked.
 * getString() unpacks the markers correctly.
 */
static bool httpJson(const char *method, const String &url, const String &body,
                     const char *h1, const String &v1,
                     const char *h2, const String &v2,
                     JsonDocument &out, JsonDocument *filter, const char *label, bool parseBody=true) {
  if(!currentRequest())return false;
  WiFiClientSecure client;
  // Core 3.x wants the size as well, 2.x does not.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  client.setCACertBundle(rootca_crt_bundle_start,
                         (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start));
#else
  client.setCACertBundle(rootca_crt_bundle_start);
  (void)rootca_crt_bundle_end;
#endif
  client.setTimeout(15);
  client.setHandshakeTimeout(15);

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);
  if (!http.begin(client, url)) { fail("%s: begin() failed", label); return false; }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  if (h1) http.addHeader(h1, v1);
  if (h2) http.addHeader(h2, v2);

  int code = (strcmp(method, "POST") == 0)
               ? http.POST((uint8_t *)body.c_str(), body.length())
               : http.GET();

  if (code <= 0) { fail("%s: %s", label, http.errorToString(code).c_str()); http.end(); return false; }
  if (code < 200 || code >= 300) {
    String e = http.getString();
    if (e.length() > 120) e = e.substring(0, 120);
    fail("%s: HTTP %d %s", label, code, e.c_str());
    http.end(); return false;
  }

  if(!parseBody) { http.end(); return true; }
  int clen = http.getSize();            // -1 = chunked
  DeserializationError e;
  if (clen >= 0) {
    e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
               : deserializeJson(out, http.getStream());
  } else {
    String payload = http.getString();
    e = filter ? deserializeJson(out, payload, DeserializationOption::Filter(*filter))
               : deserializeJson(out, payload);
  }
  http.end();
  if (e) { fail("%s: JSON %s", label, e.c_str()); return false; }
  return true;
}

bool owletLogin() {
  beginRequest();
  { StateGuard lock; gSt.loggedIn=false; }
  if (!strlen(netCfg.owletMail)) { fail("No Owlet credentials stored"); return false; }
  Serial.println("-- Owlet login --");

  String idToken;
  {
    String url = String("https://www.googleapis.com/identitytoolkit/v3/"
                        "relyingparty/verifyPassword?key=") + RK;
    JsonDocument req;
    req["email"] = netCfg.owletMail;
    req["password"] = netCfg.owletPass;
    req["returnSecureToken"] = true;
    String body; serializeJson(req, body);
    JsonDocument filter; filter["idToken"] = true;
    JsonDocument res;
    if (!httpJson("POST", url, body, "X-Android-Package", ANDROID_PKG,
                  "X-Android-Cert", ANDROID_CERT, res, &filter, "Firebase")) return false;
    idToken = res["idToken"].as<String>();
    if (idToken.length() < 100) { fail("Firebase: idToken only %u chars", idToken.length()); return false; }
    Serial.printf("   1/3 Firebase ok (%u chars)\n", idToken.length());
  }
  String mini;
  {
    // The bare JWT, with no "Bearer" in front - with Bearer the service
    // answers 401.
    JsonDocument filter; filter["mini_token"] = true;
    JsonDocument res;
    if (!httpJson("GET", RM, "", "Authorization", idToken, nullptr, "",
                  res, &filter, "Owlet SSO")) return false;
    mini = res["mini_token"].as<String>();
    if (mini.length() < 10) { fail("SSO: no mini_token"); return false; }
    Serial.printf("   2/3 SSO ok (%u chars)\n", mini.length());
  }
  idToken = String();
  {
    JsonDocument req;
    req["app_id"] = RI; req["app_secret"] = RS;
    req["provider"] = "owl_id"; req["token"] = mini;
    String body; serializeJson(req, body);
    JsonDocument filter;
    filter["access_token"] = true; filter["refresh_token"] = true; filter["expires_in"] = true;
    JsonDocument res;
    if (!httpJson("POST", RG, body, nullptr, "", nullptr, "", res, &filter, "Ayla")) return false;
    StateGuard lock;
    if(!currentRequest())return false;
    if(!res["access_token"].is<const char*>())return false;
    gAccess = res["access_token"].as<String>();
    gRefreshTok = res["refresh_token"].as<String>();
    uint32_t ttl = res["expires_in"] | 3600;
    if (!gAccess.length()) { fail("Ayla: no access_token"); return false; }
    setTokenLifetime(ttl);
    Serial.printf("   3/3 Ayla ok, token valid for %u s\n", ttl);
  }
  StateGuard lock;
  if(!currentRequest())return false;
  gSt.loggedIn = true;
  gSt.lastError[0] = 0;
  return true;
}

bool owletRefresh() {
  beginRequest();
  if(tokenGeneration!=netGeneration)return false;
  if (!gRefreshTok.length()) return false;
  JsonDocument req; req["user"]["refresh_token"] = gRefreshTok;
  String body; serializeJson(req, body);
  JsonDocument filter;
  filter["access_token"] = true; filter["refresh_token"] = true; filter["expires_in"] = true;
  JsonDocument res;
  if (!httpJson("POST", RR, body, nullptr, "", nullptr, "", res, &filter, "Refresh")) return false;
  StateGuard lock;
  if(!currentRequest() || !res["access_token"].is<const char*>())return false;
  String at = res["access_token"].as<String>();
  if (!at.length()) return false;
  gAccess = at;
  if (!res["refresh_token"].isNull()) gRefreshTok = res["refresh_token"].as<String>();
  uint32_t ttl = res["expires_in"] | 3600;
  setTokenLifetime(ttl);
  return true;
}

bool owletFindDevice() {
  beginRequest();
  { StateGuard lock; if(!gSt.loggedIn || tokenGeneration!=netGeneration)return false; }
  JsonDocument filter; filter[0]["device"]["dsn"]=true;
  JsonDocument res;
  if(!httpJson("GET",String(RB)+"/devices.json","","Authorization",String("auth_token ")+gAccess,
               nullptr,"",res,&filter,"Device list"))return false;
  JsonArray arr=res.as<JsonArray>();
  StateGuard lock;
  if(!currentRequest())return false;
  if(arr.isNull() || arr.size()==0){fail("No devices in this account");return false;}
  gSt.devices[0]=0;
  const char *chosen=nullptr;
  for(JsonObject o:arr) {
    const char *dsn=o["device"]["dsn"] | "";
    if(!*dsn || strlen(dsn)>=sizeof(gSt.dsn))continue;
    if(strlen(gSt.devices)+strlen(dsn)+3<sizeof(gSt.devices)) {
      if(*gSt.devices)strcat(gSt.devices,", ");
      strcat(gSt.devices,dsn);
    }
    if((!strlen(netCfg.owletDsn) && arr.size()==1) || !strcmp(netCfg.owletDsn,dsn))chosen=dsn;
  }
  if(!chosen){fail("Select an Owlet serial in System settings (paired: %u)",(unsigned)arr.size());return false;}
  strlcpy(gSt.dsn,chosen,sizeof(gSt.dsn));
  return true;
}

bool owletPoll() {
  beginRequest();
  char dsn[24];
  { StateGuard lock;
    if(!gSt.loggedIn || !strlen(gSt.dsn) || tokenGeneration!=netGeneration)return false;
    strlcpy(dsn,gSt.dsn,sizeof(dsn));
  }
  String auth=String("auth_token ")+gAccess;
  JsonDocument unused;
  bool active=httpJson("POST",String(RB)+"/dsns/"+dsn+"/properties/APP_ACTIVE/datapoints.json",
    "{\"datapoint\":{\"metadata\":{},\"value\":1}}","Authorization",auth,nullptr,"",unused,nullptr,"APP_ACTIVE",false);
  JsonDocument filter;
  filter[0]["property"]["name"]=true;
  filter[0]["property"]["value"]=true;
  filter[0]["property"]["data_updated_at"]=true;
  JsonDocument res;
  if(!httpJson("GET",String(RB)+"/dsns/"+dsn+"/properties.json","",
    "Authorization",auth,nullptr,"",res,&filter,"properties"))return false;
  Vitals v; char error[96];
  if(!parseVitals(res,v,error,sizeof(error))){fail("%s",error);return false;}
  StateGuard lock;
  if(!currentRequest())return false;
  acceptVitals(v,active);
  if(active) {gSt.failCount=0;gSt.lastError[0]=0;}
  else {
    strlcpy(gSt.lastError,"APP_ACTIVE failed; retrying activation",sizeof(gSt.lastError));
    if(++gSt.failCount>=5) {gSt.loggedIn=false;gSt.failCount=0;}
  }
  return true;
}
