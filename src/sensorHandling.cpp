
#include <Arduino.h>
#include <INA226.h>
#include <Wire.h>

#include "common.h"
#include "sensorHandling.h"
#include "statusHandling.h"


struct Shunt {
  float resistance;
  float maxCurrent;
};


volatile uint16_t alertCounter = 0;
double sampleTime = 0;
bool gSensorInitialized=false;

Shunt PZEM017ShuntData[4] = {
    {0.00075, 100}, {0.0015, 50}, {0.000375, 200}, {0.000250, 300}};

static INA226 ina(Wire);

IRAM_ATTR void alert(void) { ++alertCounter; }

uint16_t translateConversionTime(ina226_shuntConvTime_t time) {
    uint16_t result = 0;
    switch (time) {
        case INA226_BUS_CONV_TIME_140US:
        result = 140;
        break;
        case INA226_BUS_CONV_TIME_204US:
        result = 204;
        break;
        case INA226_BUS_CONV_TIME_332US:
        result = 332;
        break;
        case INA226_BUS_CONV_TIME_588US:
        result = 558;
        break;
        case INA226_BUS_CONV_TIME_1100US:
        result = 1100;
        break;
        case INA226_BUS_CONV_TIME_2116US:
        result = 2116;
        break;
        case INA226_BUS_CONV_TIME_4156US:
        result = 4156;
        break;
        case INA226_BUS_CONV_TIME_8244US:
        result = 8244;
        break;
        default:
        result = 0;
    }
    return result;
}

uint16_t translateSampleCount(ina226_averages_t value) {
    uint16_t result = 0;
    switch (value) {
        case INA226_AVERAGES_1:
        result = 1;
        break;
        case INA226_AVERAGES_4:
        result = 4;
        break;
        case INA226_AVERAGES_16:
        result = 16;
        break;
        case INA226_AVERAGES_64:
        result = 64;
        break;
        case INA226_AVERAGES_128:
        result = 128;
        break;
        case INA226_AVERAGES_256:
        result = 256;
        break;
        case INA226_AVERAGES_512:
        result = 512;
        break;
        case INA226_AVERAGES_1024:
        result = 1024;
        break;
        default:
        result = 0;
    }

  return result;
}

#ifdef DEBUG_SENSOR
void checkConfig() {
    Serial.print("Mode:                  ");
    switch (ina.getMode()) {
        case INA226_MODE_POWER_DOWN:
        Serial.println("Power-Down");
        break;
        case INA226_MODE_SHUNT_TRIG:
        Serial.println("Shunt Voltage, Triggered");
        break;
        case INA226_MODE_BUS_TRIG:
        Serial.println("Bus Voltage, Triggered");
        break;
        case INA226_MODE_SHUNT_BUS_TRIG:
        Serial.println("Shunt and Bus, Triggered");
        break;
        case INA226_MODE_ADC_OFF:
        Serial.println("ADC Off");
        break;
        case INA226_MODE_SHUNT_CONT:
        Serial.println("Shunt Voltage, Continuous");
        break;
        case INA226_MODE_BUS_CONT:
        Serial.println("Bus Voltage, Continuous");
        break;
        case INA226_MODE_SHUNT_BUS_CONT:
        Serial.println("Shunt and Bus, Continuous");
        break;
        default:
        Serial.println("unknown");
    }

    Serial.print("Samples average:       ");
    switch (ina.getAverages()) {
        case INA226_AVERAGES_1:
        Serial.println("1 sample");
        break;
        case INA226_AVERAGES_4:
        Serial.println("4 samples");
        break;
        case INA226_AVERAGES_16:
        Serial.println("16 samples");
        break;
        case INA226_AVERAGES_64:
        Serial.println("64 samples");
        break;
        case INA226_AVERAGES_128:
        Serial.println("128 samples");
        break;
        case INA226_AVERAGES_256:
        Serial.println("256 samples");
        break;
        case INA226_AVERAGES_512:
        Serial.println("512 samples");
        break;
        case INA226_AVERAGES_1024:
        Serial.println("1024 samples");
        break;
        default:
        Serial.println("unknown");
    }

    Serial.print("Bus conversion time:   ");
    switch (ina.getBusConversionTime()) {
        case INA226_BUS_CONV_TIME_140US:
        Serial.println("140uS");
        break;
        case INA226_BUS_CONV_TIME_204US:
        Serial.println("204uS");
        break;
        case INA226_BUS_CONV_TIME_332US:
        Serial.println("332uS");
        break;
        case INA226_BUS_CONV_TIME_588US:
        Serial.println("558uS");
        break;
        case INA226_BUS_CONV_TIME_1100US:
        Serial.println("1.100ms");
        break;
        case INA226_BUS_CONV_TIME_2116US:
        Serial.println("2.116ms");
        break;
        case INA226_BUS_CONV_TIME_4156US:
        Serial.println("4.156ms");
        break;
        case INA226_BUS_CONV_TIME_8244US:
        Serial.println("8.244ms");
        break;
        default:
        Serial.println("unknown");
    }

    Serial.print("Shunt conversion time: ");
    switch (ina.getShuntConversionTime()) {
        case INA226_SHUNT_CONV_TIME_140US:
        Serial.println("140uS");
        break;
        case INA226_SHUNT_CONV_TIME_204US:
        Serial.println("204uS");
        break;
        case INA226_SHUNT_CONV_TIME_332US:
        Serial.println("332uS");
        break;
        case INA226_SHUNT_CONV_TIME_588US:
        Serial.println("558uS");
        break;
        case INA226_SHUNT_CONV_TIME_1100US:
        Serial.println("1.100ms");
        break;
        case INA226_SHUNT_CONV_TIME_2116US:
        Serial.println("2.116ms");
        break;
        case INA226_SHUNT_CONV_TIME_4156US:
        Serial.println("4.156ms");
        break;
        case INA226_SHUNT_CONV_TIME_8244US:
        Serial.println("8.244ms");
        break;
        default:
        Serial.println("unknown");
    }

    Serial.print("Max possible current:  ");
    Serial.print(ina.getMaxPossibleCurrent());
    Serial.println(" A");

    Serial.print("Max current:           ");
    Serial.print(ina.getMaxCurrent());
    Serial.println(" A");

    Serial.print("Max shunt voltage:     ");
    Serial.print(ina.getMaxShuntVoltage());
    Serial.println(" V");

    Serial.print("Max power:             ");
    Serial.print(ina.getMaxPower());
    Serial.println(" W");

}
#endif

void sensorSetShunt(uint16_t id) {
    if(id < sizeof(PZEM017ShuntData)/ sizeof(Shunt)) {
        gShuntResistancemR = PZEM017ShuntData[id].resistance * 1000.0f;
        gMaxCurrentA = PZEM017ShuntData[id].maxCurrent;

        ina.calibrate(PZEM017ShuntData[id].resistance,PZEM017ShuntData[id].maxCurrent);
    }
    
}

void setupSensor() {
    // Default INA226 address is 0x40
    gSensorInitialized = ina.begin();

    // Check if the connection was successful, stop if not
    if (!gSensorInitialized) {
        Serial.println("Connection to sensor failed");
        
    }
    // Configure INA226
    ina.configure(INA226_AVERAGES_64, INA226_BUS_CONV_TIME_2116US,
                    INA226_SHUNT_CONV_TIME_2116US, INA226_MODE_SHUNT_BUS_CONT);
    ina.calibrate(gShuntResistancemR / 1000, gMaxCurrentA);    
    ina.enableConversionReadyAlert();

    uint16_t conversionTimeShunt =
        translateConversionTime(ina.getShuntConversionTime());
    uint16_t conversionTimeBus = translateConversionTime(
        (ina226_shuntConvTime_t)ina.getBusConversionTime());
    uint16_t samples = translateSampleCount(ina.getAverages());

    // This is the time it takes to create a new measurement
    sampleTime = (conversionTimeShunt + conversionTimeBus) * samples * 0.000001  ;
}

void sensorInit() {
    Wire.begin();
    attachInterrupt(digitalPinToInterrupt(D5), alert, FALLING);

    setupSensor();

#ifdef DEBUG_SENSOR
    // Display configuration
    checkConfig();
#endif

    gBattery.setParameters(gCapacityAh,gChargeEfficiencyPercent,gMinPercent,gTailCurrentmA,gFullVoltagemV,gFullDelayS);
}

void updateAhCounter() {
    int count;
    noInterrupts();
    // If we missed an interrupt, we assume we had thew same value
    // alle the time.
    count = alertCounter;
    alertCounter = 0;
    interrupts();

    //float shuntVoltage = ina.readShuntVoltage();
    float current = ina.readShuntCurrent();
    //Serial.printf("current is: %.2f\n",current);
    gBattery.updateConsumption(current,sampleTime,count);
    if(count > 1) {
        Serial.printf("Overflow %d\n",count);
    } 
}

void sensorLoop() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    if(!gSensorInitialized) {
        return;
    }

    if(gParamsChanged) {
        ina.calibrate(gShuntResistancemR / 1000, gMaxCurrentA);    
        gBattery.setParameters(gCapacityAh,gChargeEfficiencyPercent,gMinPercent,gTailCurrentmA,gFullVoltagemV,gFullDelayS);
    }

    while (alertCounter && ina.isConversionReady()) {           
        updateAhCounter();        
    }
    
    if(now-lastUpdate > 1000) {        
        gBattery.checkFull(ina.readBusVoltage());        
        gBattery.updateSOC();        
        gBattery.updateTtG();
        lastUpdate = now;
    }
/*
     Serial.print("Bus voltage:   ") ;
    Serial.print(ina.readBusVoltage(), 7);
    Serial.println(" V");

    Serial.print("Bus power:     ");
    Serial.print(ina.readBusPower(), 7);
    Serial.println(" W");

    Serial.print("Shunt voltage: ");
    Serial.print(ina.readShuntVoltage(), 7);
    Serial.println(" V");

    Serial.print("Shunt current: ");
    Serial.print(ina.readShuntCurrent(), 7);
    Serial.println(" A");

    Serial.println("");
*/    
}
