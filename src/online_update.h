#pragma once
#include "owlanzi.h"
#include <ArduinoJson.h>

// One flash writer at a time, including the legacy browser file upload.
enum class UpdateOwner { NONE, ONLINE, MANUAL };
bool updateClaim(UpdateOwner owner);
void updateRelease(UpdateOwner owner);
bool updateOwnedBy(UpdateOwner owner);
bool updateBusy();
bool onlineUpdateRequest(bool install);
bool onlineUpdateTick(); // network task only; true when it handled a job
void onlineUpdateJson(JsonObject out);

struct OnlineRelease {
  char version[24]="";
  char file[80]="";
  char sha256[65]="";
  uint32_t size=0;
};
const char *updateTarget();
bool newerVersion(const char *candidate,const char *current);
bool parseOnlineRelease(JsonDocument &doc,OnlineRelease &release);
