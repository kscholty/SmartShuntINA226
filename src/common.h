#pragma once

#include <Arduino.h>

#define STRING_LEN 40
#define NUMBER_LEN 32

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.

extern bool gParamsChanged;

extern uint16_t gCapacityAh;
extern uint16_t gChargeEfficiencyPercent;
extern uint16_t gMinPercent;
extern uint16_t gTailCurrentmA;
extern uint16_t gFullVoltagemV;
extern uint16_t gFullDelayS;
extern float gShuntResistancemR;
extern uint16_t gMaxCurrentA;
extern uint16_t gModbusId;
extern bool gSensorInitialized;
extern bool gModbusEanbled;
extern bool gVictronEanbled;

extern char gCustomName[64];

#define UPDATE_INTERVAL 990


#define SERIAL_DBG Serial

#if ARDUINO_USB_CDC_ON_BOOT
// This means that we have a Serial 
// and a Serial0
#define SERIAL_VICTRON Serial0
#else
#define SERIAL_VICTRON Serial
#endif
#if SOC_UART_NUM == 1
#define SERIAL_MODBUS SERIAL_VICTRON
#elif SOC_UART_NUM == 2
#define SERIAL_MODBUS Serial1
#else
#define SERIAL_MODBUS SERIAL_VICTRON
#endif
