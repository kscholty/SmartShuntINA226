
#include <Arduino.h>
#include <ModbusRTU.h>
#include "modbusHandling.h"
#include "statusHandling.h"

static ModbusRTU *modbusServer = 0;

// These are the Input Registers
// They contain current data
enum INPUT_REGISTERS {
  // The first registers are 
  // the same as for the PZEM017
  // The diefference is that this 
  // device can also measure negative
  // currents.

  REG_Voltage = 0,  
  REG_CURRENT,
  REG_POWER_LOW,
  REG_POWER_HIGH,
  REG_ENERGY_LOW,
  REG_ENERGY_HIGH,
  REG_HIGH_VOLTAGE_ALARM_STATUS,
  REG_LOW_VOLTAGE_ALARM_STATUS,
  //These registers are not part of
  //PZEM017
  REG_IDENTIFIER, // This register contains the ID 0xBF39D
  REG_TIMETOGO,
  REG_SOC,
  REG_FULL,
  REG_NUM_INPUT_REGISTERS
};

enum HOLDING_REGISTERS {
    // Also here we first have the
    // PZEM017 registers
    REG_HIGH_VOLTAGE_ALARM_THRESHOLD = 0,
    REG_LOW_VOLTAGE_ALARM_THRESHOLD = 1,
    REG_MODBUS_ADDRESS,
    REG_SHUNT_VALUE,

    //Missing yet are the other config parameters
    REG_NUM_HOLDING_REGISTERS
};


uint16_t inputGetter(uint16_t address)
{
  INPUT_REGISTERS regNum = (INPUT_REGISTERS)(address);

  switch (regNum) {
    case REG_Voltage:
      return (uint16_t)(gBattery.voltage()*100);
      break;
    case REG_CURRENT:
      return (uint16_t)(gBattery.current()*100);
      break;
    case REG_POWER_LOW:
      return (uint16_t)(gBattery.voltage()*gBattery.current()*10);
      break;
    case REG_POWER_HIGH:
      return (uint16_t) (((uint32)(gBattery.voltage()*gBattery.current()*10))>>16);
      break;
    case REG_HIGH_VOLTAGE_ALARM_STATUS:
      return 0; // Not yet implemented
      break;
    case REG_LOW_VOLTAGE_ALARM_STATUS:
      return 0; // Not yet implemented
      break;
    case REG_IDENTIFIER:
      return (uint16_t)0x93FB;
      break;
    case REG_TIMETOGO:
      return (int16_t) gBattery.tTg();
      break;
    case REG_SOC:
      return gBattery.soc();
      break;
    case REG_FULL:
      return gBattery.isFull();
      break;
      
    default:
      return UINT16_MAX;
  }
  return UINT16_MAX;
}

uint16_t holdingGetter(uint16_t address)
{
  HOLDING_REGISTERS regNum = (HOLDING_REGISTERS)(address);

  switch (regNum) {
    case REG_HIGH_VOLTAGE_ALARM_THRESHOLD:
      return (uint16_t)(UINT16_MAX); // Not yet implemented
      break;
    case REG_LOW_VOLTAGE_ALARM_THRESHOLD:
      return (uint16_t)(0); // Not yet implemented
      break;
    case REG_MODBUS_ADDRESS:
      return (uint16_t)(gModbusId);
      break;
    case REG_SHUNT_VALUE:
      return (uint16_t) (((uint32)(gBattery.voltage()*gBattery.current()*10))>>16);
      break;
      
    default:
      return UINT16_MAX;
  }
  return UINT16_MAX;
}



uint16_t setter(TRegister *reg, uint16_t val)
{
  HOLDING_REGISTERS regNum = (HOLDING_REGISTERS)(reg->address.address);

  if (reg->address.type != TAddress::RegType::HREG) {
    return 0;
  }

  switch (regNum) {
    case REG_HIGH_VOLTAGE_ALARM_THRESHOLD:
      return (uint16_t)(UINT16_MAX); // Not yet implemented
      break;
    case REG_LOW_VOLTAGE_ALARM_THRESHOLD:
      return (uint16_t)(0); // Not yet implemented
      break;
    case REG_MODBUS_ADDRESS:
        if(val != gModbusId) {
            modbusServer->server(gModbusId);        
            gModbusId = val;
            // Todo: make sure it's store in config memory.
        }
        return gModbusId;
      break;
    case REG_SHUNT_VALUE:
      if(val<4) {

        return val;
      }

      return 0;
      break;
      
    default:
      return UINT16_MAX;
  }
  return UINT16_MAX;
}


uint16_t getter(TRegister *reg, uint16_t) {
     
    switch(reg->address.type) {
        case TAddress::RegType::IREG:
            return inputGetter(reg->address.address);
            break;
        case TAddress::RegType::HREG:
            return holdingGetter(reg->address.address);
            break;

        // We don't have these    
        case TAddress::RegType::COIL:
        case TAddress::RegType::ISTS:
        default:        
            break;
        
    }
    return 0;
}


void modbusSetup()
{
  
  if (modbusServer)
  {
    delete modbusServer;
  }
  modbusServer = new ModbusRTU;
  //Config Modbus IP
  modbusServer->server(gModbusId);
  modbusServer->cbEnable(true);
  modbusServer->addIreg(0, 0, REG_NUM_INPUT_REGISTERS);
  modbusServer->addHreg(0, 0, REG_NUM_HOLDING_REGISTERS);
  modbusServer->onGet(IREG(0), getter, REG_NUM_INPUT_REGISTERS);
  modbusServer->onGet(HREG(0), getter, REG_NUM_HOLDING_REGISTERS);
  modbusServer->onSet(HREG(0), setter, REG_NUM_HOLDING_REGISTERS);
  
}

void modbusLoop(unsigned long)
{

    // poll for Modbus requests
    modbusServer->task();
}


