
#pragma once

#include <Arduino.h>
#include <RingBuf.h>


struct Statistics {
    Statistics() {
        memset(this, 0, sizeof(*this));
        secsSinceLastFull = -1;
        minBatVoltage = INT32_MAX;
    }
    float consumedAs;
    unsigned int deepestDischarge;
    unsigned int lastDischarge;
    unsigned int averageDischarge;
    unsigned int numChargeCycles;
    unsigned int numFullDischarge;
    float sumApHDrawn;
    unsigned int minBatVoltage;
    unsigned int maxBatVoltage;
    int secsSinceLastFull;
    unsigned int numAutoSyncs;
    unsigned int numLowVoltageAlarms;
    unsigned int numHighVoltageAlarms;
    float amountDischargedEnergy;
    float amountChargedEnergy;
};


class BatteryStatus {
protected:
    static const uint16_t GlidingAverageWindow = 60;
public:
    BatteryStatus();
    ~BatteryStatus() {}

    void setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV, uint16_t fullDelayS);
    void updateSOC();
    void updateTtG();
    void setVoltage(float currVoltage);
    bool checkFull();
    void updateConsumption(float current, float period, uint16_t numPeriods);
    void updateStats(unsigned long now);

    //Getters
    float tTg() {
        return tTgVal;
    }
    float soc() {
        return socVal;
    }
    bool isFull() {
        return (fullReachedAt != 0);
    }

    float voltage() {
        return lastVoltage;
    }
    float current() {
        return lastCurrent;
    }
    float averageCurrent() {
        return getAverageConsumption();
    }

    void setBatterySoc(float val);
    const Statistics& statistics() {return stats;}

    protected:                
        float getAverageConsumption();
        // This is called when we become synced for the first time;
        void resetStats();
        RingBuf<float, GlidingAverageWindow> currentValues;
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
        float tTgVal;
        float glidingAverageCurrent;
        float lastSoc;
        unsigned long lasStatUpdate;
        bool isSynced;
        Statistics stats;
};

extern BatteryStatus gBattery;
