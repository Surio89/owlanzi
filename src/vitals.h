#pragma once
#include <ArduinoJson.h>
#include "owlanzi.h"

bool parseVitals(JsonDocument &properties, Vitals &v, char *error, size_t errorSize);
uint32_t parseUtc(const char *text);
void acceptVitals(const Vitals &v, bool appActive);
