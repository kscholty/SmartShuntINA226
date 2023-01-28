This project uses an INA226 shunt amplifier to implement some smart shunt functionality.
It sums up the charges that move through the shunt. With this information it tries to calculate the load status of an attached battery.
The INA226 should be connected to the shunt so that charges going into the battery are positive and those coming out of the battery are negtive.

The smart shunt has three main interfaces.

1) A web interface for human users. It allows setting the main parameters of the system and disoplays the current status of the system
2) A modbus interface that is based on a PZEM017 energy meter but enhances it with the values mentioned above
3) Victron VE.direct Text and Hex protocols in order to function as a Battery Monitor. The Hex protocol is only implemnented as far as it's required for startup. Also some fields of the Text protocol are not yet filled correctly.

You need the version of the INA226lib https://github.com/peterus/INA226Lib The latest version should work with this code.

For measuring the current you should use an INA226 breakout board as you can acquire from Amazon, Ebay or AliExpress
https://www.amazon.de/ALAMSCN-Bi-Directional-Voltage-Current-Monitoring/dp/B09Z66QSPB/ref=sr_1_4?keywords=ina226&qid=1674921078&sr=8-4

https://de.aliexpress.com/item/1005001593541480.html?spm=a2g0o.productlist.main.3.56f351729HIcrL&algo_pvid=355d9f06-c6bf-45e7-922c-611aa36108cf&algo_exp_id=355d9f06-c6bf-45e7-922c-611aa36108cf-1&pdp_ext_f=%7B%22sku_id%22%3A%2212000016714954183%22%7D&pdp_npi=2%40dis%21EUR%213.22%212.06%21%21%21%21%21%402145294416749211574187658d06b7%2112000016714954183%21sea&curPageLogUid=PnWeLZQyi9Cc

You have to remove the resistor soldered to that board and instead use a bigger shunt, e.g. a 100A/75mV. 
Make sure that the shunt supports the current your system produces. You can set the parameters of the shunt in the web interface.
A wide variety of shunts can be found on EBay or other platforms.

If you have a 48V System, be aware of the fact that the INA226 does only support voltages up to 36V (40V max). You need a voltage divider to make shure your sensor is not destroyed. 
The code assumes that you use a 470KOhm and a 1MOhm resistor, measuring across the 1MOhm towards GND. ( + --470K-- --1M -- GND ) This should work for a 16S LifePO4 battery. The smaller you choos the small resistor in comparison to the bigger one, the more accurate the measurement will be.

The Constant 'static const float VoltageFactor'  can be used to calibrate your sensor. Set it to 1 and then simply divide the real battery voltage by the value the sensor shows. Currently this cannot be configured using the Web interface.





