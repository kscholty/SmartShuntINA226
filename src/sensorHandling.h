#pragma once


void sensorInit();
void sensorLoop();
void sensorSetShunt(uint16_t id);

extern float shuntResistance;
extern float maxExpectedCurrent;

extern uint16_t gCapacityAh;
extern uint16_t gChargeEfficiencyPercent;
extern uint16_t gMinPercent;
extern uint16_t gTailCurrentmA;
extern uint16_t gFullVoltagemV;
extern uint16_t gFullDelayS;
extern float gShuntResistancemR;
extern uint16_t gMaxCurrentA;
