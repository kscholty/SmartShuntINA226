#include "common.h"
#include "statusHandling.h"


BatteryStatus gBattery;


BatteryStatus::BatteryStatus() {
    lastCurrent = 0;
    fullReachedAt = 0;
    lastSoc = socVal = 0;
    remainAs = 0;
    consumedAs = 0;
    tTgVal = 0;
    glidingAverageCurrent = 0;    
}

void BatteryStatus::setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV,uint16_t fullDelayS) 
{

        batteryCapacity = ((float)capacityAh) *60.0f * 60.0f; // We use it in As
        chargeEfficiency = ((float)chargeEfficiencyPercent) / 100.0f;
        tailCurrent = tailCurrentmA;
        fullVoltage = fullVoltagemV;
        minAs = minPercent * batteryCapacity / 100.0f;
        fullDelay = ((unsigned long)fullDelayS) *1000;    
        //Serial.printf("Init values: Capacity %.3f, efficiency %.3f, fullDelay %ld, \n",batteryCapacity,chargeEfficiency,fullDelay);

}

void BatteryStatus::updateSOC() {
    socVal = remainAs / batteryCapacity;
    if (fabs(lastSoc - socVal) >= .01) {
        // Store value in EEPROM
        //  N.i.
        lastSoc = socVal;
    }
}

void BatteryStatus::updateTtG() {
    float avgCurrent = getAverageConsumption();
    if (avgCurrent > 0.0) {
        tTgVal = max(remainAs - minAs, 0.0f) / avgCurrent;
    }  else {
        tTgVal = INFINITY;
    }
}

void BatteryStatus::updateConsumption(float current, float period,
                                      uint16_t numPeriods) {

    // We use the average between the last and the current value for summation.
    float periodConsumption;

    for (int i = 0; i < numPeriods; ++i) {
        if (currentValues.isFull()) {
          float oldVal;
          currentValues.pop(oldVal);
          glidingAverageCurrent += oldVal;
        }
        currentValues.push(current);
        // Assumtion: Consumption is negative
        glidingAverageCurrent -= current;
    }     

    if(currentValues.isEmpty()) {
        // This is the first measurement, so we don't have an old current value
        lastCurrent = current;
    }
    
    periodConsumption = (lastCurrent + current) / 2 * period * numPeriods;
    
    if (periodConsumption > 0) {
        // We are charging
        periodConsumption *= chargeEfficiency;    
    }

    remainAs += periodConsumption;

    if(remainAs > batteryCapacity) {
        remainAs = batteryCapacity;
    } else if(remainAs < 0.0f) {
        remainAs = 0.0f;
    }
    
    consumedAs -= periodConsumption;
    lastCurrent = current;
}

float BatteryStatus::getAverageConsumption() {
    uint16_t count = currentValues.size();
    if (count != 0) {
        return  glidingAverageCurrent / count;
    } else {
        return 0;
    }
}

bool BatteryStatus::checkFull(float currVoltage) {    
    lastVoltage = currVoltage;
    if (currVoltage >= fullVoltage) {
        unsigned long now = millis();
        if(fullReachedAt == 0) {
            fullReachedAt = now;
            // This is just to indicate that we will be close to full
            if (socVal < 0.95) { setBatterySoc(0.95); }
        }
        unsigned long delay = now - fullReachedAt;
       if(delay >= fullDelay) {
            float current = getAverageConsumption();
            if (current > 0.0 && current < tailCurrent) {
                // And here we are. 100 %
                setBatterySoc(1.0);                
                return true;
            }
       }
    } else {
        fullReachedAt = 0;        
    }
    return false;
}
        

void BatteryStatus::setBatterySoc(float val) {
    socVal = val;
    remainAs = batteryCapacity * val;
    if(val>=1.0) {
        fullReachedAt = millis();
    }
    updateTtG();
}



