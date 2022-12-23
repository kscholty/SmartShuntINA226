/*
    INA226 Bi-directional Current/Power Monitor. Simple Example.
    Read more:
   http://www.jarzebski.pl/arduino/czujniki-i-sensory/cyfrowy-czujnik-pradu-mocy-ina226.html
    GIT: https://github.com/jarzebski/Arduino-INA226
    Web: http://www.jarzebski.pl
    (c) 2014 by Korneliusz Jarzebski
*/
#include <Arduino.h>
#include <INA226.h>
#include <Wire.h>

#include "common.h"
#include "sensorHandling.h"
#include "webHandling.h"
#include "modbusHandling.h"


void setup() {
  Serial.begin(115200);

    wifiSetup();
    sensorInit();
    modbusInit();
}

// Grab current value every time the alert flag is set.
// That should be every 2.116*64*2=270.848 ms
// Therefore, every time it is read, we add shuntCurrent*0,270848 As of current
// The only thing to ensure is that we don't exceed these 270.848ms in loop(), because then we could miss a value.
// Idea: Count number of interrupts between calculations.

// Offer Power and Voltage values via modbus, but don't bother to read them 
// regularely (maybe the bus voltage?).

// Store configuration in flash memory.
// USe WLAN for configuration?

void loop() {
 
 wifiLoop();
 sensorLoop();
 modbusLoop();
  
gParamsChanged = false;

}
