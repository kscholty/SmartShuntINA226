
#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>

#include "common.h"
#include "sensorHandling.h"
#include "statusHandling.h"

#if CONFIG_IDF_TARGET_ESP32S2
#define PIN_SCL SCL
#define PIN_SDA SDA
#define PIN_INTERRUPT 7
#elif defined(ESP8266)
#define PIN_SCL D1
#define PIN_SDA D2
#define PIN_INTERRUPT D5
#else
#error "Unknown Device"
#endif


static const float VoltageFactor = 2.93754; //3.17331;
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
    SERIAL_DBG_DBG.print("Mode:                  ");
    switch (ina.getMode()) {
        case INA226_MODE_POWER_DOWN:
        SERIAL_DBG.println("Power-Down");
        break;
        case INA226_MODE_SHUNT_TRIG:
        SERIAL_DBG.println("Shunt Voltage, Triggered");
        break;
        case INA226_MODE_BUS_TRIG:
        SERIAL_DBG.println("Bus Voltage, Triggered");
        break;
        case INA226_MODE_SHUNT_BUS_TRIG:
        SERIAL_DBG.println("Shunt and Bus, Triggered");
        break;
        case INA226_MODE_ADC_OFF:
        SERIAL_DBG.println("ADC Off");
        break;
        case INA226_MODE_SHUNT_CONT:
        SERIAL_DBG.println("Shunt Voltage, Continuous");
        break;
        case INA226_MODE_BUS_CONT:
        SERIAL_DBG.println("Bus Voltage, Continuous");
        break;
        case INA226_MODE_SHUNT_BUS_CONT:
        SERIAL_DBG.println("Shunt and Bus, Continuous");
        break;
        default:
        SERIAL_DBG.println("unknown");
    }

    SERIAL_DBG.print("Samples average:       ");
    switch (ina.getAverages()) {
        case INA226_AVERAGES_1:
        SERIAL_DBG.println("1 sample");
        break;
        case INA226_AVERAGES_4:
        SERIAL_DBG.println("4 samples");
        break;
        case INA226_AVERAGES_16:
        SERIAL_DBG.println("16 samples");
        break;
        case INA226_AVERAGES_64:
        SERIAL_DBG.println("64 samples");
        break;
        case INA226_AVERAGES_128:
        SERIAL_DBG.println("128 samples");
        break;
        case INA226_AVERAGES_256:
        SERIAL_DBG.println("256 samples");
        break;
        case INA226_AVERAGES_512:
        SERIAL_DBG.println("512 samples");
        break;
        case INA226_AVERAGES_1024:
        SERIAL_DBG.println("1024 samples");
        break;
        default:
        SERIAL_DBG.println("unknown");
    }

    SERIAL_DBG.print("Bus conversion time:   ");
    switch (ina.getBusConversionTime()) {
        case INA226_BUS_CONV_TIME_140US:
        SERIAL_DBG.println("140uS");
        break;
        case INA226_BUS_CONV_TIME_204US:
        SERIAL_DBG.println("204uS");
        break;
        case INA226_BUS_CONV_TIME_332US:
        SERIAL_DBG.println("332uS");
        break;
        case INA226_BUS_CONV_TIME_588US:
        SERIAL_DBG.println("558uS");
        break;
        case INA226_BUS_CONV_TIME_1100US:
        SERIAL_DBG.println("1.100ms");
        break;
        case INA226_BUS_CONV_TIME_2116US:
        SERIAL_DBG.println("2.116ms");
        break;
        case INA226_BUS_CONV_TIME_4156US:
        SERIAL_DBG.println("4.156ms");
        break;
        case INA226_BUS_CONV_TIME_8244US:
        SERIAL_DBG.println("8.244ms");
        break;
        default:
        SERIAL_DBG.println("unknown");
    }

    SERIAL_DBG.print("Shunt conversion time: ");
    switch (ina.getShuntConversionTime()) {
        case INA226_SHUNT_CONV_TIME_140US:
        SERIAL_DBG.println("140uS");
        break;
        case INA226_SHUNT_CONV_TIME_204US:
        SERIAL_DBG.println("204uS");
        break;
        case INA226_SHUNT_CONV_TIME_332US:
        SERIAL_DBG.println("332uS");
        break;
        case INA226_SHUNT_CONV_TIME_588US:
        SERIAL_DBG.println("558uS");
        break;
        case INA226_SHUNT_CONV_TIME_1100US:
        SERIAL_DBG.println("1.100ms");
        break;
        case INA226_SHUNT_CONV_TIME_2116US:
        SERIAL_DBG.println("2.116ms");
        break;
        case INA226_SHUNT_CONV_TIME_4156US:
        SERIAL_DBG.println("4.156ms");
        break;
        case INA226_SHUNT_CONV_TIME_8244US:
        SERIAL_DBG.println("8.244ms");
        break;
        default:
        SERIAL_DBG.println("unknown");
    }

    SERIAL_DBG.print("Max possible current:  ");
    SERIAL_DBG.print(ina.getMaxPossibleCurrent());
    SERIAL_DBG.println(" A");

    SERIAL_DBG.print("Max current:           ");
    SERIAL_DBG.print(ina.getMaxCurrent());
    SERIAL_DBG.println(" A");

    SERIAL_DBG.print("Max shunt voltage:     ");
    SERIAL_DBG.print(ina.getMaxShuntVoltage());
    SERIAL_DBG.println(" V");

    SERIAL_DBG.print("Max power:             ");
    SERIAL_DBG.print(ina.getMaxPower());
    SERIAL_DBG.println(" W");

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
        SERIAL_DBG.println("Connection to sensor failed");
        
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
    Wire.begin(PIN_SDA,PIN_SCL); 
    attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT), alert, FALLING);

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
    //SERIAL_DBG.printf("current is: %.2f\n",current);
    gBattery.updateConsumption(current,sampleTime,count);
    if(count > 1) {
        SERIAL_DBG.printf("Overflow %d\n",count);
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
        gBattery.setVoltage(ina.readBusVoltage() * VoltageFactor);
    }
    
    if (now - lastUpdate > 1000) {
        gBattery.checkFull();        
        gBattery.updateSOC();        
        gBattery.updateTtG();
        gBattery.updateStats(now);
        lastUpdate = now;
    }
/*
     SERIAL_DBG.print("Bus voltage:   ") ;
    SERIAL_DBG.print(ina.readBusVoltage(), 7);
    SERIAL_DBG.println(" V");

    SERIAL_DBG.print("Bus power:     ");
    SERIAL_DBG.print(ina.readBusPower(), 7);
    SERIAL_DBG.println(" W");

    SERIAL_DBG.print("Shunt voltage: ");
    SERIAL_DBG.print(ina.readShuntVoltage(), 7);
    SERIAL_DBG.println(" V");

    SERIAL_DBG.print("Shunt current: ");
    SERIAL_DBG.print(ina.readShuntCurrent(), 7);
    SERIAL_DBG.println(" A");

    SERIAL_DBG.println("");
*/    
}
