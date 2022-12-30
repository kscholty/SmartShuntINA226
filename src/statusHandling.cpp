#include "common.h"
#include "statusHandling.h"


BatteryStatus gBattery;


BatteryStatus::BatteryStatus() {
    lastCurrent = 0;
    fullReached = 0;
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
    socVal = max(remainAs, 0.0f) / batteryCapacity;

    //Serial.printf("SocVal %.3f capacity: %.2f",socVal,batteryCapacity);
    // We are not close to full, so limit the Soc to 95%"""
    if (socVal > 0.95 && !fullReached) {
        setBatterySoc(0.95);
    }

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
        if(remainAs<0.0) {
            // It seems we switch from discharging to charging
            // with assumed very low load status. So let's assume the battery is empty...
            remainAs = 0.0;
        }
    }

    remainAs += periodConsumption;
    
    consumedAs -= periodConsumption;
    lastCurrent = current;

    //Serial.printf("Consumption: Current: %.3f period cons %.3f, glidingAvg %.3f remain As %.3f\n",current,periodConsumption,glidingAverageConsumption, remainAs);
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
    //Serial.printf("Voltage is: %.2f\n",currVoltage);
    lastVoltage = currVoltage;
    if (currVoltage >= fullVoltage) {
        unsigned long now = millis();
        if(fullReached == 0) {
            fullReached = now;
            if (socVal < 0.95) { setBatterySoc(0.95); }
        }
        unsigned long delay = now - fullReached;
       if(delay >= fullDelay) {
            float current = getAverageConsumption();
            if (current > 0.0 && current < tailCurrent) {
                setBatterySoc(1);
                //Serial.println("Full reached");
                return true;
            }
       }
    } else {
        fullReached = 0;
        //Serial.println("Full not reached");
    }
    return false;
}
        

void BatteryStatus::setBatterySoc(float val) {
    socVal = val;
    remainAs = batteryCapacity * val;
    updateTtG();
}



