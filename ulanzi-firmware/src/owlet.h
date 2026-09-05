#pragma once
#include "owlanzi.h"

bool owletLogin();       // Firebase -> SSO -> Ayla
bool owletRefresh();     // Token verlaengern, ohne die ganze Kette
bool owletFindDevice();  // Seriennummer holen
bool owletPoll();        // APP_ACTIVE + properties.json -> gSt.v
uint32_t owletTokenSecondsLeft();
