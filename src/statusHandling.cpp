#include "common.h"
#include "statusHandling.h"




BatteryStatus gBattery;


BatteryStatus::BatteryStatus() {
    lastCurrent = 0;
    fullReachedAt = 0;
    lastSoc = 0;
    glidingAverageCurrent = 0;
    lasStatUpdate = 0;
    isSynced = false;
    if (!readStatusFromRTC()) {
        stats.init();
    }
}

void BatteryStatus::setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV,uint16_t fullDelayS) 
{

        batteryCapacity = ((float)capacityAh) *60.0f * 60.0f; // We use it in As
        chargeEfficiency = ((float)chargeEfficiencyPercent) / 100.0f;
        tailCurrent = tailCurrentmA / 1000.0f;
        fullVoltage = fullVoltagemV / 1000.0f;
        minAs = minPercent * batteryCapacity / 100.0f;
        fullDelay = ((unsigned long)fullDelayS) *1000;    
        //SERIAL_DBG.printf("Init values: Capacity %.3f, efficiency %.3f, fullDelay %ld, \n",batteryCapacity,chargeEfficiency,fullDelay);

}

void BatteryStatus::updateSOC() {
    stats.socVal = stats.remainAs / batteryCapacity;
    if (fabs(lastSoc - stats.socVal) >= .005) {
        // Store value in RTC memory
        writeStatusToRTC();
        lastSoc = stats.socVal;        
    }
}

void BatteryStatus::updateTtG() {
    float avgCurrent = getAverageConsumption();
    if (avgCurrent > 0.0) {
        stats.tTgVal = max(stats.remainAs - minAs, 0.0f) / avgCurrent;
    }  else {
        stats.tTgVal = INFINITY;
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
    
    periodConsumption = (lastCurrent + current) / 2.0 * period * numPeriods;

    // Has to be in 0.01 kWh....
    float consumption = periodConsumption / 3.6 / 1000.0 / 10.0 * lastVoltage;
    if (periodConsumption > 0) {
        // We are charging
        stats.amountChargedEnergy += consumption;
        periodConsumption *= chargeEfficiency;
    } else {
        stats.sumApHDrawn += periodConsumption / -3.6;
        stats.amountDischargedEnergy -= consumption;
    }

    
    stats.remainAs += periodConsumption;
    stats.consumedAs += periodConsumption;
    
    if (stats.remainAs > batteryCapacity) {
        stats.remainAs = batteryCapacity;
    } else if(stats.remainAs < 0.0f) {
        stats.remainAs = 0.0f;
    }
    
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
void BatteryStatus::setVoltage(float currVoltage) {
    lastVoltage = currVoltage;
}

bool BatteryStatus::checkFull() {
    if (lastVoltage >= fullVoltage) {
        
        if (stats.socVal < 0.95) {
            // This is just to indicate that we will be close to full
            setBatterySoc(0.95);
        }
        float current = -1 * getAverageConsumption();
        if (current > 0.0 && current <= tailCurrent) {
            unsigned long now = millis();
            if (fullReachedAt == 0) {
                fullReachedAt = now;
            }
            unsigned long delay = now - fullReachedAt;
            if (delay >= fullDelay) {
                // And here we are. 100 %
                setBatterySoc(1.0);
                if (!isSynced) {
                    resetStats();
                    isSynced = true;
                }
                stats.secsSinceLastFull = 0;
                stats.numAutoSyncs++;
                stats.lastDischarge = roundf(stats.remainAs / 3.6);
                stats.consumedAs = 0.0;
                return true;
            }
        } else {
            fullReachedAt = 0;
        }
    } else {
        fullReachedAt = 0;
    }
    return false;
}


void BatteryStatus::setBatterySoc(float val) {
    stats.socVal = val;
    stats.remainAs = batteryCapacity * val;
    if(val>=1.0) {
        fullReachedAt = millis();
    }
    updateTtG();
}


void BatteryStatus::resetStats() {
    stats.deepestDischarge = stats.remainAs / 3.6;
}


void BatteryStatus::updateStats(unsigned long now) {
    int timeDeltaSec = (now - lasStatUpdate) / 1000;
    lasStatUpdate = now;
    if (timeDeltaSec < 0) {
        // We had an overflow, so let's assume the last call was 1 sec ago (default interval)
        timeDeltaSec = 1;
    }
    if (stats.secsSinceLastFull >= 0) {
        stats.secsSinceLastFull += timeDeltaSec;
    }


    if (stats.tTgVal != INFINITY) {
        float mAh = stats.consumedAs / 3.6;
        if (stats.deepestDischarge > mAh) {
            stats.deepestDischarge = roundf(mAh);
        }
        
        stats.lastDischarge = roundf(mAh);
        stats.averageDischarge = stats.lastDischarge;
    }

    uint32_t voltageV = lastVoltage * 1000;
    if (stats.minBatVoltage > voltageV) {
        stats.minBatVoltage = voltageV;
    }
    if (stats.maxBatVoltage < voltageV) {
        stats.maxBatVoltage = voltageV;
    }    
}


#ifdef ESP32
RTC_DATA_ATTR Statistics rtcStats;

void BatteryStatus::writeStatusToRTC() {
    memcpy(&rtcStats, &stats, sizeof(stats));
}

bool BatteryStatus::readStatusFromRTC() {
    bool res = true;
    if (rtcStats.magic == MAGICKEY) {
         memcpy(&stats, &rtcStats, sizeof(stats));
    } else {
        res = false;
    }
    return res;
}

#else 
void BatteryStatus::writeStatusToRTC() {
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&stats, sizeof(stats));
}

bool BatteryStatus::readStatusFromRTC() {
    uint32_t magic = 0;
    if (!ESP.rtcUserMemoryRead(0, &magic, sizeof(magic)) || magic != MAGICKEY) {
        return false;
    }

    if (!ESP.rtcUserMemoryRead(0, (uint32_t*)&stats.magic, sizeof(stats))) {
        Serial.println("RTC read failed!");
        return false;
    }

    return true;
}
#endif
