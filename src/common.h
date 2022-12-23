#pragma once

#include <Arduino.h>

#define STRING_LEN 40
#define NUMBER_LEN 32

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
static const char thingName[] = "INR SmartShunt";

extern bool gParamsChanged;
