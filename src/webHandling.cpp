

#define DEBUG_WIFI(m) Serial.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>

#include <ESP8266WiFi.h>      

#include <time.h>
//needed for library
#include <DNSServer.h>

#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include "common.h"
#include "statusHandling.h"

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "B1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Method declarations.
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper*);

DNSServer dnsServer;
WebServer server(80);

bool gParamsChanged = true;

char capacityAhTxt[NUMBER_LEN] = "100";
uint16_t gCapacityAh;

char chargeEfficiencyPercentTxt[NUMBER_LEN] = "95";
uint16_t gChargeEfficiencyPercent;

char minPercentTxt[NUMBER_LEN] = "10";
uint16_t gMinPercent;

char tailCurrentmATxt[NUMBER_LEN] = "2000";
uint16_t gTailCurrentmA;

char fullVoltagemVTxt[NUMBER_LEN] = "55200";
uint16_t gFullVoltagemV;

char FullDelaySTxt[NUMBER_LEN] = "30";
uint16_t gFullDelayS;

char shuntResistancemVTxt[NUMBER_LEN] = "0.75";
float gShuntResistancemV;

char maxCurrentATxt[NUMBER_LEN] = "200";
uint16_t gMaxCurrentA;

char modbusIdTxt[NUMBER_LEN] = "1";
uint16_t gModbusId;


IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

IotWebConfParameterGroup sysConfGroup = IotWebConfParameterGroup("SysConf","Sensor");
IotWebConfNumberParameter shuntResistance = IotWebConfNumberParameter("Shunt resistance [m&#8486;]", "shuntR", shuntResistancemVTxt, NUMBER_LEN, shuntResistancemVTxt);
IotWebConfNumberParameter maxCurrent = IotWebConfNumberParameter("Expected max current [A]", "maxA", maxCurrentATxt, NUMBER_LEN, maxCurrentATxt);

IotWebConfParameterGroup shuntGroup = IotWebConfParameterGroup("ShuntConf","Smart shunt");
IotWebConfNumberParameter battCapacity = IotWebConfNumberParameter("Battery capacity [Ah]", "battAh", capacityAhTxt, NUMBER_LEN, capacityAhTxt);
IotWebConfNumberParameter chargeEfficiency = IotWebConfNumberParameter("Charge efficiency [%]", "cheff", chargeEfficiencyPercentTxt, NUMBER_LEN, chargeEfficiencyPercentTxt);
IotWebConfNumberParameter minSoc = IotWebConfNumberParameter("Minimun SOC [%]", "minsoc", minPercentTxt, NUMBER_LEN, minPercentTxt);

IotWebConfParameterGroup fullGroup = IotWebConfParameterGroup("FullD","Full detection");
IotWebConfNumberParameter tailCurrent = IotWebConfNumberParameter("Tail current [mA]", "tailC", tailCurrentmATxt, NUMBER_LEN, tailCurrentmATxt);
IotWebConfNumberParameter fullVoltage = IotWebConfNumberParameter("Voltage when full [mV]", "fullV", fullVoltagemVTxt, NUMBER_LEN, fullVoltagemVTxt);
IotWebConfNumberParameter fullDelay = IotWebConfNumberParameter("Delay before full [s]", "fullDelay", FullDelaySTxt, NUMBER_LEN, FullDelaySTxt);

IotWebConfParameterGroup modbusGroup = IotWebConfParameterGroup("modbus","Modbus settings");
IotWebConfNumberParameter modbusId = IotWebConfNumberParameter("MOdbus Id", "mbid", modbusIdTxt, NUMBER_LEN, modbusIdTxt);




void wifiConnected()
{
   ArduinoOTA.begin();
}

void wifiSetup()
{
  
  sysConfGroup.addItem(&shuntResistance);
  sysConfGroup.addItem(&maxCurrent);

  shuntGroup.addItem(&battCapacity);
  shuntGroup.addItem(&chargeEfficiency);
  shuntGroup.addItem(&minSoc);

  fullGroup.addItem(&fullVoltage);
  fullGroup.addItem(&tailCurrent);
  fullGroup.addItem(&fullDelay);
  
  modbusGroup.addItem(&modbusId);
  
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);

  iotWebConf.addParameterGroup(&sysConfGroup);
  iotWebConf.addParameterGroup(&shuntGroup);
  iotWebConf.addParameterGroup(&fullGroup);
  iotWebConf.addParameterGroup(&modbusGroup);

  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);


  iotWebConf.setFormValidator(formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- Initializing the configuration.
  iotWebConf.init();
  
  Serial.println("Converting params");
  convertParams();
  Serial.println("Values are:");
  Serial.printf("Resistance: %.2f\n",gShuntResistancemV);
  Serial.printf("Capacity %df\n",gCapacityAh);
  Serial.printf("MOdbus ID %df\n",gModbusId);
  

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() { iotWebConf.handleNotFound(); });
}

void wifiLoop()
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  ArduinoOTA.handle();

/*
  if(gNeedReset) {
      Serial.println("Rebooting after 1 second.");
      iotWebConf.delay(1000);
      ESP.restart();
  }
  */
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }

  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";

  s += "<title>INR based smart shunt</title></head><body>";
  
  s += "<br><br><b>Config Values</b> <ul>";
  s += "<li>Shunt resistance  : "+String(gShuntResistancemV);
  s += "<li>Shunt max current : "+String(gMaxCurrentA);
  s += "<li>Batt capacity     : "+String(gCapacityAh);
  s += "<li>Batt efficiency   : "+String(gChargeEfficiencyPercent);
  s += "<li>Min soc           : "+String(gMinPercent);
  s += "<li>Tail current      : "+String(gTailCurrentmA);
  s += "<li>Batt full voltage : "+String(gFullVoltagemV);
  s += "<li>Batt full delay   : "+String(gFullDelayS);
  s += "<li>Modbus ID         : "+String(gModbusId);
  s += "</ul><hr><br>";
  
  s += "<br><br><b>Dynamic Values</b> <ul>";
  s += "<li>Battery Voltage: "+String(gBattery.voltage());
  s += "<li>Shunt current  : "+String(gBattery.current());
  s += "<li>Avg current    : "+String(gBattery.averageCurrent());
  s += "<li>Battery soc    : "+String(gBattery.soc());
  s += "<li>Time to go     : "+String(gBattery.tTg());
  s += "<li>Battery full   : "+String(gBattery.isFull());
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change configuration.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}


void convertParams() {
    gShuntResistancemV = server.arg(shuntResistance.getId()).toFloat();
    gMaxCurrentA = server.arg(maxCurrent.getId()).toInt();
    gCapacityAh = server.arg(battCapacity.getId()).toInt();
    gChargeEfficiencyPercent = server.arg(chargeEfficiency.getId()).toInt();
    gMinPercent = server.arg(minSoc.getId()).toInt();
    gTailCurrentmA = server.arg(tailCurrent.getId()).toInt();
    gFullVoltagemV = server.arg(fullVoltage.getId()).toInt();
    gFullDelayS = server.arg(fullDelay.getId()).toInt();
    gModbusId = server.arg(modbusId.getId()).toInt();
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  convertParams();
  gParamsChanged = true;
} 



bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form.");
  bool result = true;

  int l = 0;

  
    if (server.arg(shuntResistance.getId()).toFloat() <=0)
    {
        shuntResistance.errorMessage = "Shunt resistance has to be > 0";
        result = false;
    }

    l = server.arg(maxCurrent.getId()).toInt();
    if ( l <= 0)
    {
        maxCurrent.errorMessage = "Maximal current must be > 0";
        result = false;
    }

    l = server.arg(battCapacity.getId()).toInt();
    if (l <= 0)
    {
        battCapacity.errorMessage = "Battery capacity must be > 0";
        result = false;
    }

    l = server.arg(chargeEfficiency.getId()).toInt();
    if (l <= 0  || l> 100) {
        chargeEfficiency.errorMessage = "Charge efficiency must be between 1% and 100%";
        result = false;
    }


    l = server.arg(minSoc.getId()).toInt();
    if (l <= 0  || l> 100) {
        minSoc.errorMessage = "Minimum SOC must be between 1% and 100%";
        result = false;
    }

    l = server.arg(tailCurrent.getId()).toInt();
    if (l < 0 ) {
        tailCurrent.errorMessage = "Tail current must be > 0";
        result = false;
    }

    l = server.arg(fullVoltage.getId()).toInt();
    if (l < 0 ) {
        fullVoltage.errorMessage = "Voltage when full must be > 0";
        result = false;
    }

    l = server.arg(fullDelay.getId()).toInt();
    if (l < 0 ) {
        fullDelay.errorMessage = "Delay before full must be > 0";
        result = false;
    }

  return result;
  }
