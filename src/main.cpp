/*
    INA226 Bi-directional Current/Power Monitor. Simple Example.
    Read more:
   http://www.jarzebski.pl/arduino/czujniki-i-sensory/cyfrowy-czujnik-pradu-mocy-ina226.html
    GIT: https://github.com/jarzebski/Arduino-INA226
    Web: http://www.jarzebski.pl
    (c) 2014 by Korneliusz Jarzebski
*/
#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>

#include "common.h"
#include "sensorHandling.h"
#include "webHandling.h"
#include "modbusHandling.h"
#include "victronHandling.h"


void setup() {
#if ARDUINO_USB_CDC_ON_BOOT
    SERIAL_VICTRON.begin(19200, SERIAL_8N1, RX, TX);
    // there seems to be a bug in the Arduine core that 
    // prevents RX from working. The next line fixes that....
    SERIAL_VICTRON.setPins(RX, TX, -1, -1);
    SERIAL_DBG.begin(115200);
#else
    SERIAL_DBG.begin(19200);
#endif
    
    wifiSetup();

#if (SOC_UART_NUM > 1)
    SERIAL_MODBUS.begin(9600, SERIAL_8N2);
#endif

    sensorInit();
    modbusInit();
    victronInit();
}

void loop() {

    wifiLoop();
    if (gParamsChanged) {
        modbusInit();
        victronInit();
    }
    sensorLoop();
    modbusLoop();
    victronLoop();
    gParamsChanged = false;

}
