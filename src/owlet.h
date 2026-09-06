#pragma once
#include "owlanzi.h"

bool owletLogin();       // Firebase -> SSO -> Ayla
bool owletRefresh();     // extend the token without the whole chain
bool owletFindDevice();  // fetch the serial number
bool owletPoll();        // APP_ACTIVE + properties.json -> gSt.v
uint32_t owletTokenSecondsLeft();
