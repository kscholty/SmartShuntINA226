
#pragma once

#include <Arduino.h>
#include <RingBuf.h>


class BatteryStatus {
    protected:
        static const uint16_t GlidingAverageWindow=30;
    public:
        BatteryStatus() ;
        ~BatteryStatus() {}

        void setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV,uint16_t fullDelayS);
        void updateSOC();
        void updateTtG();
        bool checkFull(float currVoltage);
        void updateConsumption(float current,float period, uint16_t numPeriods);

        //Getters
        float tTg() {return tTgVal;}
        float soc() {return socVal;}
        bool isFull() { return (fullReachedAt != 0);}

        float voltage() {return lastVoltage;}
        float current() {return lastCurrent;}
        float averageCurrent() {return getAverageConsumption();}

        void setBatterySoc(float val);
        
    protected:                
        float getAverageConsumption();
        RingBuf<float,GlidingAverageWindow> currentValues;
        float batteryCapacity;
        float chargeEfficiency; // Value between 0 and 1 (representing percent)       
        float tailCurrent; // For full detection, A going ointo the battery
        float fullVoltage; // Voltage when Battery ois assumed to be full
        float minAs; // Amount of As that are in the battery when we assume it to be empty
        unsigned long fullDelay; // For how long do we need Full Voltage and current < tailCurrent to assume battery is full

        float lastVoltage;
        float lastCurrent;        
        unsigned long fullReachedAt;
        float socVal;
        float remainAs;
        float consumedAs;
        float tTgVal;
        float glidingAverageCurrent;
        float lastSoc;
};

extern BatteryStatus gBattery;
