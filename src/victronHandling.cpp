
#include <Arduino.h>

#include "common.h"
#include "statusHandling.h"

// This is a SmartShunt 500A
static const uint16_t PID = 0xA389;
static const uint16_t AppId = 0b0100000100100000;  // 0x4120
static const unsigned long UART_TIMEOUT = 900;


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
    READ_DATA,
    READ_STRING,
    COMPLETE,
    EXECUTE
};

enum ANSWERS {
    ANSWER_DONE = 1,
    ANSWER_UNKNOWN = 3,
    ANSWER_PING = 5,
    ANSWER_GET = 7,
    ANSWER_SET = 8
};

enum FLAGS {
    FLAG_OK = 0x0,
    FLAG_UNKNOWN_ID = 0x1,
    FLAG_NOT_SUPPORTED = 0x2,
    FLAG_PARAMETER_ERROR = 0x4
};

void sendAnswer(uint8_t* bytes, uint8_t count) {
    uint8_t checksum = bytes[0];
    SERIAL_VICTRON.write(':');
    // This is the command, just 1 nibble
    SERIAL_VICTRON.printf("%hhX", bytes[0]);
    for (int i = 1; i < count; ++i) {
        checksum += bytes[i];
        SERIAL_VICTRON.printf("%02hhX", bytes[i]);
    }
    checksum = 0x55 - checksum;
    SERIAL_VICTRON.printf("%02hhX", checksum);
    SERIAL_VICTRON.write('\n');
}

typedef void (*CommandFunc)(uint8_t, uint16_t, uint8_t, uint8_t*, uint8_t);

void commandPing(uint8_t command, uint16_t, uint8_t, uint8_t*, uint8_t) {
    uint8_t answer[] = { 5, AppId & 0xFF, AppId >> 8 };
    sendAnswer(answer, sizeof(answer));
}

void commandAppVersion(uint8_t command, uint16_t, uint8_t, uint8_t*, uint8_t) {
    uint8_t answer[] = { 1, AppId & 0xFF, AppId >> 8 };
    sendAnswer(answer, sizeof(answer));
}

void commandProductId(uint8_t command, uint16_t, uint8_t, uint8_t*, uint8_t) {
    uint8_t answer[] = { 1, PID & 0xFF, PID >> 8 };
    sendAnswer(answer, sizeof(answer));
}

void commandRestart(uint8_t command, uint16_t, uint8_t, uint8_t*, uint8_t) {
    // Ignore for now
}

void commandGet(uint8_t command, uint16_t address, uint8_t flags, uint8_t*,
    uint8_t) {
    uint8_t answer[128];
    uint8_t aSize = 4;
    size_t len;
    char serialnr[32];
    answer[0] = COMMAND_GET;
    answer[1] = (uint8_t)address;
    answer[2] = (uint8_t)(address >> 8);
    answer[3] = FLAG_OK;
    answer[4] = 0;
    answer[5] = 0;

    switch (address) {
        case 0xEEB8:
            aSize = 6;
            break;
        case 0x034F:
            aSize = 5;
            break;
        case 0x010A:
#if ESP32
            sprintf(serialnr, "%08X", ESP.getEfuseMac());
#else
            sprintf(serialnr, "%08X", ESP.getChipId());
#endif
            
            len = strlen(serialnr);
            aSize = 4 + len;
            memcpy(answer + 4, serialnr, len);
            break;
        case 0x010C:
            len = strlen(gCustomName);
            aSize = 4 + len;
            memcpy(answer + 4, gCustomName, len);
            break;
        case 0x0104:
            // GroupId. Used to group similar devices together...
            // u8. We use 0, since I have no better idea
            aSize = 5;
            break;
        default:
            answer[3] = FLAG_UNKNOWN_ID;
            // SERIAL_DBG.printf("Unknown GET address 0x%X\r\n",address);
            break;
    }

    sendAnswer(answer, aSize);
}

void commandSet(uint8_t command, uint16_t address, uint8_t flags,
    uint8_t* valueBuf, uint8_t valueSize) {
    uint8_t answer[128];

    answer[0] = COMMAND_SET;
    answer[1] = (uint8_t)address;
    answer[2] = (uint8_t)(address >> 8);
    answer[3] = FLAG_OK;
    memcpy(answer + 4, valueBuf, valueSize);

    switch (address) {
        case 0x010C:
            //Value should be astring
            // This can be set
            memcpy(gCustomName, valueBuf, valueSize);
            gCustomName[valueSize] = 0;
            break;
        default:
            answer[3] = FLAG_UNKNOWN_ID;
            break;
    }

    sendAnswer(answer, valueSize + 4); 
}

void commandUnknown(uint8_t command, uint16_t, uint8_t, uint8_t*, uint8_t) {
    uint8_t answer[2] = { ANSWER_UNKNOWN, command };

     SERIAL_DBG.printf("Unknown command: %d\r\n",command);
    // Unknown command received
    sendAnswer(answer, sizeof(answer));
}

CommandFunc commandHandlers[] = {
    commandUnknown,   commandPing,    commandUnknown, commandAppVersion,
    commandProductId, commandUnknown, commandRestart, commandGet,
    commandSet, commandUnknown, commandUnknown };

void victronInit() {
    if (gVictronEanbled) {
        if (SERIAL_VICTRON.baudRate() != 19200) {
            SERIAL_VICTRON.updateBaudRate(19200); 
        }
    }
}

uint8_t calcChecksum(const String& s) {
    int len = s.length();
    uint8_t result = 0;
    for (int i = 0; i < len; ++i) {
        result += s[i];
    }
    return 256u - result;
}

void sendSmallBlock() {
    int intVal;
    const Statistics& stats = gBattery.statistics();
    
    String S = "\r\nPID\t0x" + String(PID, 16);
    intVal = roundf(gBattery.voltage() * 1000);
    S += "\r\nV\t" + String(intVal, 10);
    intVal = roundf(gBattery.current() * 1000);
    S += "\r\nI\t" + String(intVal, 10);
    intVal = roundf(gBattery.current() * gBattery.voltage());
    S += "\r\nP\t" + String(intVal, 10);
    intVal = roundf(stats.consumedAs / 3.6);
    S += "\r\nCE\t" + String(intVal);
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
    S += "\r\nMON\t" + String(gVictronDevice) ;
    S += "\r\nChecksum\t";

    uint8_t cs = calcChecksum(S);
    SERIAL_VICTRON.write(S.c_str());
    SERIAL_VICTRON.write(cs);
}

void sendHistoryBlock() {
    const Statistics& stats = gBattery.statistics();
    int intVal;
    String S = "\r\nH1\t" + String(stats.deepestDischarge);
    S += "\r\nH2\t" + String(stats.lastDischarge);
    S += "\r\nH3\t" + String(stats.averageDischarge);
    S += "\r\nH4\t" + String(stats.numChargeCycles);
    S += "\r\nH5\t" + String(stats.numFullDischarge);
    intVal = roundf(stats.sumApHDrawn);
    S += "\r\nH6\t" + String(intVal);
    S += "\r\nH7\t" + String(stats.minBatVoltage);
    S += "\r\nH8\t" + String(stats.maxBatVoltage);
    S += "\r\nH9\t";
    if (stats.secsSinceLastFull < 0) {
        S += "---";
    } else {
        S += String(stats.secsSinceLastFull);
    }
    S += "\r\nH10\t" + String(stats.numAutoSyncs);
    S += "\r\nH11\t" + String(stats.numLowVoltageAlarms);
    S += "\r\nH12\t" + String(stats.numHighVoltageAlarms);
    intVal = roundf(stats.amountDischargedEnergy);
    S += "\r\nH17\t" + String(intVal);
    intVal = roundf(stats.amountChargedEnergy);
    S += "\r\nH18\t" + String(intVal);

    S += "\r\nChecksum\t";
    uint8_t cs = calcChecksum(S);
    SERIAL_VICTRON.write(S.c_str());
    SERIAL_VICTRON.write(cs);
}

#define char2int(VAL) ((VAL) > '@' ? ((VAL) & 0xDF) - 'A' + 10 : (VAL) - '0')

bool readByte(uint8_t& value) {
    char result[2];
    int read;
    read = SERIAL_VICTRON.readBytes(result,1);
    if (read == 1) {
        if (result[0] < '0') {
            value = (uint8_t)result[0];
            return true;
        }
        read = SERIAL_VICTRON.readBytes(result+1,1);
        if (read == 1) {
            value = (uint8_t)((char2int(result[0]) << 4) | char2int(result[1]));
            return true;
        }
    }

    SERIAL_DBG.printf("readByte: Read failure read %d %c %c\r\n", read, result[0], result[1]);
    return false;
}


void rxData(unsigned long now) {
    static STATE status = IDLE;
    static uint8_t command;
    static uint16_t address;
    static uint8_t flags;
    static uint8_t checksum;
    static uint8_t currIndex;
    static uint8_t valueBuffer[64];

    uint8_t inbyte;
    bool ok;

    while (true) {
        //SERIAL_DBG.printf("Status is: %d\r\n",status);
        if (status != IDLE && (now - lastHexCmdMillis > UART_TIMEOUT)) {
            status = IDLE;
            lastHexCmdMillis = 0;
        }

        switch (status) {
            case IDLE:
                inbyte = SERIAL_VICTRON.read();
                //SERIAL_DBG.printf("%x\r\n",inbyte); 
                if (inbyte == ':') {
                    lastHexCmdMillis = now;
                    // A new command starts
                    checksum = 0;
                    command = COMMAND_UNKNOWN;
                    flags = 0;
                    address = 0;
                    status = READ_COMMAND;
                }
                return;
            case READ_COMMAND:
                inbyte = SERIAL_VICTRON.read();
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
                ok = readByte(valueBuffer[1]);
                if (ok) {
                    checksum += valueBuffer[1];
                    ok = readByte(valueBuffer[0]);
                }
                if (ok) {
                    checksum += valueBuffer[0];
                    address = valueBuffer[0] << 8 | valueBuffer[1];
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
                currIndex = 0;                                
                status = READ_DATA;
                // Fall through                
            case READ_DATA:                       
                ok = readByte(valueBuffer[currIndex]);
                //SERIAL_DBG.printf("Read %X\r\n", valueBuffer[currIndex]);
                if (ok) {
                    if (valueBuffer[currIndex] == '\r') {                        
                        return;
                    }
                    if (valueBuffer[currIndex] == '\n') {
                        // End of command
                        // We already read the checksum as the last character 
                        // Overwrite it with 0 (if it's a string it's correctly terminated then)
                        valueBuffer[--currIndex] = 0;
                        if (checksum != 0x55) {
                            // This is an error
                            status = IDLE;
                        } else {
                            status = EXECUTE;
                        }                        
                        break;
                    }
                    checksum += valueBuffer[currIndex++];
                    if (currIndex >= sizeof(valueBuffer)) {
                        // This message is too long for us....
                        status = IDLE;
                    }
                } else {
                    // Error while reading from UART
                    SERIAL_DBG.printf("Read failed (ix %d, checksum %x)\r\n",currIndex,checksum);
                    status = IDLE;
                }
                return;
            case READ_CHECKSUM:
                ok = readByte(inbyte);
                if (!ok || (uint8_t)(checksum + inbyte) != 0x55) {
                    // Checksum failure
                    // Just ignore this
                     SERIAL_DBG.printf("Ok: %d Checksum failure %X MY sum is %X\r\n",ok,inbyte,checksum);
                    status = IDLE;
                } else {
                    status = COMPLETE;
                }
                return;
            case COMPLETE:
                inbyte = SERIAL_VICTRON.read();
                if (inbyte == '\r') return;
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
                    commandHandlers[command](command, address, flags, valueBuffer, currIndex);
                } else {
                    commandUnknown(command, address, flags, valueBuffer, currIndex);
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
    static unsigned long lastSentHistory = millis();
    unsigned long now;
    bool stopText;

    now = millis();

    if (gVictronEanbled) {
        while (SERIAL_VICTRON.available()) {
            //SERIAL_DBG.println("Data available");
            rxData(now);
        }

        stopText = ((lastHexCmdMillis > 0) && (now - lastHexCmdMillis < UPDATE_INTERVAL));
        if (!stopText && (now - lastSent >= UPDATE_INTERVAL)) {
            SERIAL_DBG.print(".");
            sendSmallBlock();
            lastSent = now;
            lastHexCmdMillis = 0;
            if (now - lastSentHistory >= UPDATE_INTERVAL * 10) {
                SERIAL_DBG.println("*");
                sendHistoryBlock();
                lastSentHistory = now;
            }
        }
    }
}
