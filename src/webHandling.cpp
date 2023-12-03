

#define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>
#if ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>      
#endif

#include <time.h>
//needed for library
#include <DNSServer.h>

#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <IotWebConfTParameter.h>

#include "common.h"
#include "statusHandling.h"

#define SOC_RESPONSE \
"<!DOCTYPE HTML>\
    <html> <head>\
            <meta charset=\"UTF-8\">\
            <meta http-equiv=\"refresh\" content=\"3; url=/\">\
            <script type=\"text/javascript\">\
                window.location.href = \"/\"\
            </script>\
            <title>SOC set</title>\
        </head>\
        <body>\
            Soc has been updated <br><a href='/'>Back</a>\
        </body>\
    </html>\n"

#define SOC_FORM \
"<hr><br>\
  <b>Set state of charge</b><br><br>\
  <form action=\"/setsoc\" method=\"POST\">\
  <div>\
  <label for=\"soc\">New battery soc: </label>\
  <input name=\"soc\" id=\"soc\" value=\"100\" />\
  <button type=\"submit\">Set</button></div> <br><br>"

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "B2"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif

// -- Method declarations.
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper*);

DNSServer dnsServer;
WebServer server(80);

bool gParamsChanged = true;

uint16_t gCapacityAh;

uint16_t gChargeEfficiencyPercent;

uint16_t gMinPercent;

uint16_t gTailCurrentmA;

uint16_t gFullVoltagemV;

uint16_t gFullDelayS;

float gShuntResistancemR;

uint16_t gMaxCurrentA;

uint16_t gModbusId;

bool gModbusEanbled = false;

bool gVictronEanbled = true;

char gCustomName[64] = "INR SmartShunt";

// -- We can add a legend to the separator
IotWebConf iotWebConf(gCustomName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

IotWebConfParameterGroup sysConfGroup = IotWebConfParameterGroup("SysConf","Sensor");

iotwebconf::FloatTParameter shuntResistance =
   iotwebconf::Builder<iotwebconf::FloatTParameter>("shuntR").
   label("Shunt resistance [m&#8486;]").
   defaultValue(0.75f).
   placeholder("e.g. 0.75").
   build();

iotwebconf::UIntTParameter<uint16_t> maxCurrent =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("maxA").
  label("Expected max current [A]").
  defaultValue(200).
  min(1u).
  step(1u).
  placeholder("1..65535").
  build();


IotWebConfParameterGroup shuntGroup = IotWebConfParameterGroup("ShuntConf","Smart shunt");

iotwebconf::UIntTParameter<uint16_t> battCapacity =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("battAh").
  label("Battery capacity [Ah]").
  defaultValue(100).
  min(1).
  step(1).
  placeholder("1..65535").
  build();

iotwebconf::UIntTParameter<uint16_t> chargeEfficiency =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("cheff").
  label("Charge efficiency [%]").
  defaultValue(95).
  min(1).
  max(100).
  step(1).
  placeholder("1..100").
  build();

iotwebconf::UIntTParameter<uint16_t> minSoc =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("minsoc").
  label("Minimun SOC [%]").
  defaultValue(10).
  min(1).
  max(100).
  step(1).
  placeholder("1..100").
  build();

IotWebConfParameterGroup fullGroup = IotWebConfParameterGroup("FullD","Full detection");

iotwebconf::UIntTParameter<uint16_t> tailCurrent =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("tailC").
  label("Tail current [mA]").
  defaultValue(1000).
  min(1).
  step(1).
  placeholder("1..65535").
  build();


iotwebconf::UIntTParameter<uint16_t> fullVoltage =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("fullV").
  label("Voltage when full [mV]").
  defaultValue(55200).
  min(1).
  step(1).
  placeholder("1..65535").
  build();

iotwebconf::UIntTParameter<uint16_t> fullDelay =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("fullDelay").
  label("Delay before full [s]").
  defaultValue(30).
  min(1).
  step(1).
  placeholder("1..65535").
  build();


IotWebConfParameterGroup communicationGroup = IotWebConfParameterGroup("comm","Communication settings");
iotwebconf::UIntTParameter<uint16_t> modbusId =
  iotwebconf::Builder<iotwebconf::UIntTParameter<uint16_t>>("mbid").
  label("Modbus Id").
  defaultValue(2).
  min(1).
  max(128).
  step(1).
  placeholder("1..128").
  build();

static const char protocolValues[][STRING_LEN] = { "m", "v", "n" };
static const char protocolNames[][STRING_LEN] = { "Modbus", "Victron", "None" };


iotwebconf::SelectTParameter<STRING_LEN> protocolChooserParam =
   iotwebconf::Builder<iotwebconf::SelectTParameter<STRING_LEN>>("prot").
   label("Communication protocol").
   optionValues((const char*)protocolValues).
   optionNames((const char*)protocolNames).
   optionCount(sizeof(protocolValues) / STRING_LEN).
   nameLength(STRING_LEN).
   defaultValue("v").
   build();

iotwebconf::TextTParameter<sizeof(gCustomName)> nameParam =
iotwebconf::Builder<iotwebconf::TextTParameter<sizeof(gCustomName)>>("name").
label("Name").
defaultValue(gCustomName).
build();

void wifiSetShuntVals() {
    shuntResistance.value() = gShuntResistancemR;
    maxCurrent.value() = gMaxCurrentA;
}

void wifiSetModbusId() {
    modbusId.value() = gModbusId;
}

void wifiStoreConfig() {
    iotWebConf.saveConfig();
}


void wifiConnected()
{
   ArduinoOTA.begin();
}

void onSetSoc() {
    String soc = server.arg("soc");
    soc.trim();
    if(!soc.isEmpty()) {
        uint16_t socVal = soc.toInt();
        gBattery.setBatterySoc(((float)socVal)/100.0);
        //Serial.printf("Set soc to %.2f",gBattery.soc());
    }

    server.send(200, "text/html", SOC_RESPONSE);
} 


void handleSetRuntime() {
String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";  
  s += "<title>Set runtime data</title></head><body>";
  s += SOC_FORM;
  s += "<UL><LI>Go to <a href='config'>configure page</a> to change configuration.";
  s += "<LI>Go to <a href='/'>main page</a></UL>";

  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiSetup()
{
  shuntResistance.customHtml = "min='0.001' max='10.0' step='0.001'";

  sysConfGroup.addItem(&shuntResistance);
  sysConfGroup.addItem(&maxCurrent);

  shuntGroup.addItem(&battCapacity);
  shuntGroup.addItem(&chargeEfficiency);
  shuntGroup.addItem(&minSoc);

  fullGroup.addItem(&fullVoltage);
  fullGroup.addItem(&tailCurrent);
  fullGroup.addItem(&fullDelay);

  // communication settings

  communicationGroup.addItem(&nameParam);
  communicationGroup.addItem(&protocolChooserParam);
  communicationGroup.addItem(&modbusId);


  
  iotWebConf.setStatusPin(STATUS_PIN,ON_LEVEL);
  iotWebConf.setConfigPin(CONFIG_PIN);

  iotWebConf.addParameterGroup(&sysConfGroup);
  iotWebConf.addParameterGroup(&shuntGroup);
  iotWebConf.addParameterGroup(&fullGroup);
  iotWebConf.addParameterGroup(&communicationGroup);

  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);


  iotWebConf.setFormValidator(formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- Initializing the configuration.
  iotWebConf.init();
  
  convertParams();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() { iotWebConf.handleNotFound(); });
  
  server.on("/setruntime", handleSetRuntime);
  server.on("/setsoc",HTTP_POST,onSetSoc);
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
  s += "<meta http-equiv=\"refresh\" content=\"3; url=/\">";
  s += "<title>"+String(gCustomName)+"</title></head><body>";
  
  s += "<br><br><b>Config Values</b> <ul>";
  s += "<li>Shunt resistance  : " + String(gShuntResistancemR, 4) + " m&#8486;";
  s += "<li>Shunt max current : " + String(gMaxCurrentA, 3) + " A";
  s += "<li>Batt capacity     : " + String(gCapacityAh) + " Ah";
  s += "<li>Batt efficiency   : " + String(gChargeEfficiencyPercent) + " %";
  s += "<li>Min soc           : " + String(gMinPercent) + " %";
  s += "<li>Tail current      : " + String(gTailCurrentmA) + " mA";
  s += "<li>Batt full voltage : " + String(gFullVoltagemV) + " mV";
  s += "<li>Batt full delay   : " + String(gFullDelayS) + " s";
  s += "<li>Name              : " + String(gCustomName);
  s += "<li>Modbus enabled    : " + String(gModbusEanbled ? "true" : "false");
  s += "<li>Victron enabled   : " + String(gVictronEanbled ? "true" : "false");
  s += "<li>Modbus ID         : " + String(gModbusId);
  s += "</ul><hr><br>";

  s += "<br><b>Dynamic Values</b>";
  
  if (gSensorInitialized) {
    s += "<ul> <li>Battery Voltage: " + String(gBattery.voltage()) + " V";
    s += "<li>Shunt current  : " + String(gBattery.current(),3) + " A";
    s += "<li>Avg consumption: " + String(gBattery.averageCurrent(),3) + " A";
    s += "<li>Battery soc    : " + String(gBattery.soc(),3);
    s += "<li>Time to go     : " + String(gBattery.tTg()) + " s";
    s += "<li>Battery full   : " + String(gBattery.isFull()?"true":"false");
    s += "</ul>";
  } else {
    s += "<br><div><font color=\"red\" size=+1><b>Sensor failure!</b></font></div><br>";
  }
  
  s += "<UL><LI>Go to <a href='config'>configure page</a> to change configuration.";
  s += "<LI>Go to <a href='setruntime'>runtime modification page</a> to change runtime data.</UL>";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}


void convertParams() {
    gShuntResistancemR = shuntResistance.value();
    gMaxCurrentA = maxCurrent.value();
    gCapacityAh = battCapacity.value();
    gChargeEfficiencyPercent = chargeEfficiency.value();
    gMinPercent = minSoc.value();
    gTailCurrentmA = tailCurrent.value();
    gFullVoltagemV = fullVoltage.value();
    gFullDelayS = fullDelay.value();
    gModbusId = modbusId.value();
    gModbusEanbled = strcmp(protocolChooserParam.value(),"m") == 0; 
    gVictronEanbled = strcmp(protocolChooserParam.value(), "v") == 0;
    strcpy(gCustomName, nameParam.value());
}

void configSaved()
{ 
  convertParams();
  gParamsChanged = true;
} 



bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{ 
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
