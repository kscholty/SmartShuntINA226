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
  Serial.begin(9600,SERIAL_8N1);

    wifiSetup();
    sensorInit();
    modbusInit();
}

void loop() {
 
 wifiLoop();
 sensorLoop();
 modbusLoop();
  
gParamsChanged = false;

}
