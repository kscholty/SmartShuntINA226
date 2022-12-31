This project uses an INA226 shunt amplifier to implement some smart shunt functionality.
It sums up the charges that move through the shunt. With this information it tries to calculate the load status of an attached battery.
The INA226 should be connected to the shunt so that charges going into the battery are positive and those coming out of the battery are negtive.

The smart shunt has two main interfaces.

1) A web interface for human users. It allows setting the main parameters of the system and disoplays the current status of the system
2) A modbus interface that is based on a PZEM017 energy meter but enhances it with the values mentioned above

You need the version of the INA226lib from here as well. My changes have not yet been incorporated upstream.
