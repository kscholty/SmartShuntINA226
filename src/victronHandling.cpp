
#include <Arduino.h>
#include "common.h"
#include "statusHandling.h"


// This is a SmartShunt 500A
static const uint16_t PID = 0xA389;
static const uint16_t AppId= 0b0100000100100000; // 0x4120
static const unsigned long UART_TIMEOUT = 1000; 
static const char *DESCRIPTION= "INR226 SmartShunt";
static const char *SERIALNR= "1234567890";

static unsigned long lastHexCmdMillis = 0;


enum STATE {
    IDLE = 0,
    COMMAND_PING = 1,
    COMMAND_APP_VERSION = 3,
    COMMAND_PRODUCT_ID = 4,
    COMMAND_RESTART = 6,
    COMMAND_GET = 7,
    COMMAND_SET = 8, 
    COMMAND_ASYNC = 0xA,
    COMMAND_UNKNOWN,    
    NUM_COMMANDS,
    READ_COMMAND,
    READ_CHECKSUM,
    READ_REGISTER,
    READ_FLAGS,
    READ_VALUE,
    COMPLETE,
    EXECUTE
};

enum ANSWERS {
    ANSWER_DONE=1,
    ANSWER_UNKNOWN = 3,
    ANSWER_PING = 5,
    ANSWER_GET = 7,
    ANSWER_SET=8
};

enum FLAGS {
    FLAG_OK = 0x0,
    FLAG_UNKNOWN_ID = 0x1,
    FLAG_NOT_SUPPORTED = 0x2,
    FLAG_PARAMETER_ERROR = 0x4
};


void sendAnswer(uint8_t *bytes, uint8_t count) {
    uint8_t checksum = bytes[0];    
    Serial.write(':');
    // This is the command, just 1 nibble
    Serial.printf("%hhX",bytes[0]);        
    for(int i=1;i<count;++i) {
        checksum += bytes[i];
        Serial.printf("%02hhX",bytes[i]);        
    }
    checksum = 0x55 - checksum;
    Serial.printf("%02hhX",checksum);    
    Serial.write('\n');
}

typedef void (*CommandFunc)(uint8_t, uint16_t, uint8_t, uint16_t ) ;

void commandPing(uint8_t command, uint16_t, uint8_t, uint16_t) {
    uint8_t answer[] = {5, AppId & 0xFF, AppId >> 8};
    sendAnswer(answer, sizeof(answer));
}

void commandAppVersion(uint8_t command, uint16_t, uint8_t, uint16_t) {
    uint8_t answer[] = {1, AppId & 0xFF, AppId >> 8};
    sendAnswer(answer, sizeof(answer));
}

void commandProductId(uint8_t command, uint16_t, uint8_t, uint16_t ) {
    uint8_t answer[] = {1, PID & 0xFF, PID >> 8};
    sendAnswer(answer, sizeof(answer));
}

void commandRestart(uint8_t command, uint16_t, uint8_t, uint16_t ) {
    // Ignore for now
}

void commandGet(uint8_t command, uint16_t address, uint8_t flags, uint16_t ) {
    uint8_t answer[64];
    uint8_t aSize = 4;
    size_t len;
    answer[0]= COMMAND_GET;
    answer[1] = (uint8_t)address;
    answer[2] = (uint8_t)(address >> 8);
    answer[3] = FLAG_OK;
    answer[4] = 0;
    answer[5] = 0;

    switch(address) {
        case 0xEEB8:            
            aSize = 6;
        break;
        case 0x034F: 
            aSize = 5;
            break;
        case 0x010A: 
            len = strlen(SERIALNR)+1;
            aSize = 4+len;
            memcpy(answer+4,SERIALNR,len);
            break;
        case 0x010C: 
            len = strlen(DESCRIPTION)+1;
            aSize = 4+len;
            memcpy(answer+4,DESCRIPTION,len);
            break;
        case 0x0104:
            // GroupId. Used to group similar devices together...
            // u8. We use 0, since I have no better idea
            aSize = 5;
            break;
        default:
            answer[3] = FLAG_UNKNOWN_ID;
            //Serial.printf("Unknown GET address 0x%X\r\n",address);
            break;
    }

    sendAnswer(answer, aSize);
}

void commandSet(uint8_t command, uint16_t address, uint8_t flags, uint16_t value) {
    uint8_t answer[64];
    uint8_t aSize = 4;
    size_t len;
    answer[0]= COMMAND_SET;
    answer[1] = (uint8_t)address;
    answer[2] = (uint8_t)(address >> 8);
    answer[3] = FLAG_OK;
    answer[4] = (uint8_t)value;
    answer[5] = (uint8_t)(value >> 8);

    switch(address) {
        case 0x010C:
        // This can be set
        default:
            answer[3] = FLAG_UNKNOWN_ID;
            break;
    }

    sendAnswer(answer, sizeof(answer));
}

void commandUnknown(uint8_t command, uint16_t, uint8_t, uint16_t) {
    uint8_t answer[2] = {ANSWER_UNKNOWN,command};
    // Unknown command received    
    sendAnswer(answer, sizeof(answer));
}

CommandFunc commandHandlers[] = {
    commandUnknown, commandPing, commandUnknown, commandAppVersion, 
    commandProductId, commandUnknown, commandRestart, commandGet, commandSet,
    commandUnknown, commandUnknown};


void victronInit() {   
    if(gVictronEanbled) {
        Serial.updateBaudRate(19200);
    }
}

uint8_t calcChecksum(const String &s) {
    int len = s.length();
    uint8_t result = 0;
    for (int i = 0; i < len; ++i) {
        result += s[i];
    }
    return 256u - result;
}

void sendSmallBlock() {
    int intVal;
    String S = "\r\nPID\t0x" + String(PID, 16);
    intVal = roundf(gBattery.voltage() * 1000);
    S += "\r\nV\t" + String(intVal, 10);
    intVal = roundf(gBattery.current() * 1000);
    S += "\r\nI\t" + String(intVal, 10);
    intVal = roundf(gBattery.current() * gBattery.voltage());
    S += "\r\nP\t" + String(intVal, 10);
    S += "\r\nCE\t---";  // Maybe we add a counter for this later. Up to now we
                         // don't have that info
    intVal = roundf(gBattery.soc() * 1000);
    S += "\r\nSOC\t" + String(intVal, 10);
    if (gBattery.tTg() == INFINITY) {
        intVal = -1;
    } else {
        intVal = roundf(gBattery.tTg() / 60);
    }
    S += "\r\nTTG\t" + String(intVal, 10);
    S += "\r\nAlarm\tOFF";
    S += "\r\nRelay\tOFF";
    S += "\r\nAR\t0";
    S += "\r\nAR\t0";
    S += "\r\nBMV\tINR226";
    S += "\r\nFW\t" + String(AppId, 16);
    S += "\r\nChecksum\t";

    uint8_t cs = calcChecksum(S);
    Serial.write(S.c_str());
    Serial.write(cs);
}

void sendHistoryBlock() {
   
}

#define char2int(VAL) ((VAL) > '@' ? ((VAL) & 0xDF)-'A'+10:(VAL)-'0')

bool readByte(uint8_t &value) {
    char result[2];
    size_t read;

    read = Serial.readBytes(result,2);    
    if(read != 2) {
        return false;
    }
    value = (char2int(result[0])<<4)|char2int(result[1]);
    return true;
}

void rxData(unsigned long now) {
    static STATE status = IDLE;
    static uint8_t command;
    static uint16_t address;
    static uint8_t flags;
    static uint16_t value;
    static uint8_t checksum;
    
    uint8_t inbyte;
    uint8_t bytes[2];            
    bool ok;

    while (true) {
        //Serial.printf("Status is: %d\r\n",status);
        if(status != IDLE && (now - lastHexCmdMillis > UART_TIMEOUT)) {
            status = IDLE;
            lastHexCmdMillis = 0;
        }
        switch (status) {
          case IDLE:
            inbyte = Serial.read();
            
            if (inbyte == ':') {                
                lastHexCmdMillis = now;
              // A new command starts
              checksum = 0;
              command = COMMAND_UNKNOWN;
              flags = 0;
              address = 0;
              value = 0;
              status = READ_COMMAND;
            }
            return;
          case READ_COMMAND:
            inbyte = Serial.read();
            command = char2int(inbyte);
            checksum += command;            
            if (command < NUM_COMMANDS) {
              status = (STATE)command;             
            } else {
              // This is an unknow command. Trigger error mnessage
              status = EXECUTE;
            }
            // Continue in the loop
            break;
          case COMMAND_PING:
          case COMMAND_APP_VERSION:
          case COMMAND_PRODUCT_ID:
          case COMMAND_RESTART:
            status = READ_CHECKSUM;
            // Return to wait for more input
            return;
          case COMMAND_GET:
          case COMMAND_SET:
            status = READ_REGISTER;
            return;
            break;
          case READ_REGISTER:
            ok = readByte(bytes[1]);
            if (ok) {
              checksum += bytes[1];
              ok = readByte(bytes[0]);
            }
            if (ok) {
              checksum += bytes[0];
              address = bytes[0] << 8 | bytes[1];
              status = READ_FLAGS;
            } else {
              status = IDLE;
            }
            return;
          case READ_FLAGS:
            ok = readByte(flags);
            checksum += flags;
            if (ok) {
              status = (command == COMMAND_GET ? READ_CHECKSUM : READ_VALUE);
            } else {
              status = IDLE;
            }
            return;
          case READ_VALUE:
            ok = readByte(bytes[1]);
            if (ok) {
              checksum += bytes[1];
              ok = readByte(bytes[0]);
            }
            if (ok) {
              checksum += bytes[0];
              value = bytes[0] << 8 | bytes[1];
              status = READ_CHECKSUM;
            } else {
              status = IDLE;
            }
            return;
          case READ_CHECKSUM:
            ok = readByte(inbyte);
            if (!ok || (uint8_t)(checksum + inbyte) != 0x55) {
              // Checksum failure
              // Just ignore this
              //Serial.printf("Ok: %d Checksum failure %X MY sum is %X\r\n",ok,inbyte,checksum);
              status = IDLE;
            } else {
              status = COMPLETE;
            }
            return;
          case COMPLETE:
            inbyte = Serial.read();
            if(inbyte == '\r') return;
            if (inbyte == '\n') {
              status = EXECUTE;
            } else {
              // This would be a violation of the protocol
              status = IDLE;
              return;
            }
            break;
          case EXECUTE:
            if (command < NUM_COMMANDS) {
              commandHandlers[command](command, address, flags, value);
            } else {
              commandUnknown(command, address, flags, value);
            }
            status = IDLE;
            return;
          case COMMAND_ASYNC:
            status = IDLE;
            return;
          default:
            status = IDLE;
            return;
        }
    }
}

void victronLoop() {
    static unsigned long lastSent = 0;
    static unsigned long lastSentBig = millis();
    unsigned long now;
    bool stopText;

    now = millis();

    if (gVictronEanbled) {
        while (Serial.available()) {
          rxData(now);
        }

        stopText = (lastHexCmdMillis > 0) && (now - lastHexCmdMillis < 2000);
        if (!stopText && (now - lastSent > 1000)) {
          sendSmallBlock();
          lastSent = now;
          lastHexCmdMillis = 0;
          if (now - lastSentBig > 3000) {
            sendHistoryBlock();
            lastSentBig = now;
          }
        }
    }
}
