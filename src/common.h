#pragma once

#include <Arduino.h>

#define STRING_LEN 40
#define NUMBER_LEN 32

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
static const char thingName[] = "INRSmartShunt";

extern bool gParamsChanged;

extern uint16_t gCapacityAh;
extern uint16_t gChargeEfficiencyPercent;
extern uint16_t gMinPercent;
extern uint16_t gTailCurrentmA;
extern uint16_t gFullVoltagemV;
extern uint16_t gFullDelayS;
extern float gShuntResistancemV;
extern uint16_t gMaxCurrentA;
extern uint16_t gModbusId;
