## Overview
This project uses an INA226 shunt amplifier to implement some smart shunt functionality.
It sums up the charges that move through the shunt. With this information it tries to calculate the load status of an attached battery.
The INA226 should be connected to the shunt so that charges going into the battery are positive and those coming out of the battery are negtive.

The smart shunt has three main interfaces.

1) A web interface for human users. It allows setting the main parameters of the system and displays the current status of the system
2) A modbus interface that is based on a PZEM017 energy meter but enhances it with the values mentioned above
3) Victron VE.direct Text and Hex protocols in order to function as a Battery Monitor. The Hex protocol is only implemnented as far as it's required for startup. Also some fields of the Text protocol are not yet filled correctly.

## Building the code yourself
The Software has been created using platformio and the Arduino environment. In order to build it you als need some libraries.
* the [INA226lib](https://github.com/peterus/INA226Lib) The latest version should work with this code.
* emelianov/modbus-esp8266
* locoduino/RingBuffer
* prampec/IotWebConf

The latter three will be automatically downloaded when using platformio.

## Required hardware

For measuring the current you need an __INA226 breakout board__ as you can acquire from 
[Amazon](https://www.amazon.de/ALAMSCN-Bi-Directional-Voltage-Current-Monitoring/dp/B09Z66QSPB/ref=sr_1_4?keywords=ina226&qid=1674921078&sr=8-4),
[Ebay](https://www.ebay.de/itm/403798012528?mkcid=16&mkevt=1&mkrid=707-127634-2357-0&ssspo=3VTGCNeFTJm&sssrc=2047675&ssuid=0YZwUxrsQgu&widget_ver=artemis&media=COPY) or [AliExpress](https://de.aliexpress.com/item/1005001593541480.html?spm=a2g0o.productlist.main.3.56f351729HIcrL&algo_pvid=355d9f06-c6bf-45e7-922c-611aa36108cf&algo_exp_id=355d9f06-c6bf-45e7-922c-611aa36108cf-1&pdp_ext_f=%7B%22sku_id%22%3A%2212000016714954183%22%7D&pdp_npi=2%40dis%21EUR%213.22%212.06%21%21%21%21%21%402145294416749211574187658d06b7%2112000016714954183%21sea&curPageLogUid=PnWeLZQyi9Cc)

I noticed that some of the boards have a different pinout. The SDA and SDL pins are swapped in comparison to others. So please check this if you can't get a connection to the sensor.

As __miocrocontroller__ I settled for Espressif products. This code has been tested with a Wemos D1 Mini, a Wemos S2 mini and a 8266 Nodemcu. I didn't test any ESP32 WROOM boards, but they should work too. I setteled for the S2 mini due to its small footprint and powerful CPU. I got mine from [Aliexpress](https://de.aliexpress.com/item/1005004438665554.html?spm=a2g0o.productlist.main.5.5ffa60aafadABW&algo_pvid=dd20a2a6-e95b-45fa-a21f-18e0922615da&algo_exp_id=dd20a2a6-e95b-45fa-a21f-18e0922615da-2&pdp_ext_f=%7B%22sku_id%22%3A%2212000029182790427%22%7D&pdp_npi=2%40dis%21EUR%212.72%212.23%21%21%21%21%21%402145274c16754348326558450d06ca%2112000029182790427%21sea&curPageLogUid=wBPd7zKYNEX1)

You also need a __shunt__, since the small resistor on the board (dont forget to remove it!) will not survive the high currents of a PV plant.
There are plenty of options out there. Again [Aliexpress](https://de.aliexpress.com/item/1005001678592758.html?spm=a2g0o.productlist.main.29.306d4ec19gJkAJ&algo_pvid=29bbcfa0-d3c6-4854-8a79-1bb33adbf3d4&aem_p4p_detail=2023020306362112631641799161540004060030&algo_exp_id=29bbcfa0-d3c6-4854-8a79-1bb33adbf3d4-14&pdp_ext_f=%7B%22sku_id%22%3A%2212000017093083607%22%7D&pdp_npi=2%40dis%21EUR%213.88%213.1%21%21%21%21%21%402100b84516754349819031656d0753%2112000017093083607%21sea&curPageLogUid=4Pyd35DHXJ61&ad_pvid=2023020306362112631641799161540004060030_4&ad_pvid=2023020306362112631641799161540004060030_4) or [Ebay](https://www.ebay.de/sch/i.html?_from=R40&_trksid=p2334524.m570.l1313&_nkw=shunt+resistor&_sacat=0&LH_TitleDesc=0&_odkw=shunt&_osacat=0) are probably the best sources.
Make sure that you select a resistor appropriate for the currents you expect. I'm using a 200A/75mV shunt.

I recommend using a __buck converter__ to create 5V for feeding the MCU and the sensor from the battery. 
Make sure Battery - is connected to the GND potential of the sensor. Otherwise the voltage measurements will be invalid, since the INA226 has only one GND.
Furthermore, you should use an __isolated USB module__ to connect the D1 to the target. You can destroy your computer or Victron GSX if you connect it directly to the USB of the MCU. [I use this one](https://www.ebay.de/itm/164934927888?mkcid=16&mkevt=1&mkrid=707-127634-2357-0&ssspo=k80Mu6A-TbK&sssrc=2047675&ssuid=0YZwUxrsQgu&widget_ver=artemis&media=COPY)

__Before you can use the sensor board you have to remove the shunt resistor soldered to that board and instead use a bigger shunt, e.g. a 100A/75mV.__
Make sure that the shunt supports the current your system produces. You can set the parameters of the shunt in the web interface.
A wide variety of shunts can be found on EBay or other platforms.

If you have a 48V System, be aware of the fact that the INA226 does only support voltages up to 36V (40V max). You need a voltage divider to make shure your sensor is not destroyed. 
The code assumes that you use a __470KOhm and a 1MOhm__ resistor, measuring across the 1MOhm towards GND. `( + --470K-- --1M -- GND )` This should work for a 16S LifePO4 battery. The smaller you choose the small resistor in comparison to the bigger one, the more accurate the measurement will be.

The constant `static const float VoltageFactor`  can be used to calibrate your sensor. Set it to `1` and then simply divide the real battery voltage by the value the sensor shows. Currently this cannot be configured using the web interface.

The following image shows how to connect the parts.
![Breadboard](https://github.com/kscholty/SmartShuntINA226/blob/master/Schema/SmartShunt_Steckplatine.png)


## Interfaces:

1) Web Interface. 
    The web interface is quite self explanatory. It contains values to configure the shunt you are using. 
    Furthermore some that have been inspired by the Victron SmartShunt. 
2) Victron Text and Hex Protocols. These are decirbed on the  Victron Website and are mainly useful for connecting to Victron Cerbos or othe GX devices.
3)  The Modbus interface
    The Modbus interface uses 9600 Buad 8N2. The following registers are exposed
    - Holding registers (the first 4 are the ones from a PZEM-017)
    ```
        0: High Voltage alarm Threshold (not functional at the moment)
        1: Low Voltage alarm Threshold (not functional at the moment)
        2: Modbus Address
        3: Shunt Value (Refer to table below) what the values mean
        4: Identifier (This register contains the ID 0xBF39D to distinguish it from other sensors)
        5: Set SOC (Can be used to set an SOC, e.g. after startup)
    ```
    - Input Registers (again the first 8 are identical to the PZEM-017, however, here the current and power can be negative)
    ```
        0: Bus Voltage
        1: Current
        2: PowerLow (power low word)
        3: PowerHigh (power high word)
        4: EnergyLow (Energy low word)
        5: EnergyHigh (Energy high word)
        6: HighVoltageAlarm status (Is high voltage alarm set, not yet functional)
        7: LowVoltageAlarm status (Is high voltage alarm set, not yet functional)
        8: TimeToGoLow (LowWord of timeToGo in Seconds)
        9: TimeToGoHigh (HighWord of timeToGo in Seconds)
        10: SOC (Soc in %)
        11: isFull (1 if battery is detected to be full, 0 otherwise)
    ```

Shunt values for modbus, assumed is a voltage of 75mV at nominal current. 
The values are those used by PZEM-017.

```
Register Value | nominal current | Resulting resistance mR
0              |  100A           |  0.750
1              |   50A           |  1.500
2              |  200A           |  0.375
3              |  300A           |  0.250
```

When you start the sensor for the first time it will open an access point you can connect to in order to set wifi credentials and the other parameters.
After connecting to that access point, direct your browser to `http://192.168.4.1`. This will open the configuration page.
The sensor will always create that access point after startup. For how long can be configured. 


## Screenshots

## Remote console with Shunt
![Console1](/Schema/RemoteConsoleMain.png)

## Remote console details
![Console2](/Schema/RemoteConsoleDevice.png)

## Web config
![Web config](/Schema/WebConfig.png)

## Everything soldered together
![Prototype](/Schema/prototype.jpg)




