# GNSS library
Diagnostics module can be used to both record GNSS data coming from the GNSS hardware chip, or disconnect the GNSS chip and inject GNSS data to the GNSS driver in the firmware. Doing so will enable developers to test firmware on their own desks, and also enable the hardware in the loop test setup to perform simulated position sequences and automatically test the behavior. 

## gnss_recorder.py
Script for recording GNSS data. Uses nfdiag and Diagnostics module GNSS sniffer mode to collect GNSS data from the GNSS chip. Data is recorded to file and can be played back in e.g. U-center. 

## gnss_simulator 
Script for simulating GNSS data and injecting data to the firmware.

**WiP: See m10sim.py description**

## m10sim.py
Implements a crude and minimal simulator of the commands and responses in the MIA-M10 chip as used on the collar hardware. It utilizes the pyubx2 library for interpreting and building UBX protocol packets. NMEA is not supported. 

**TODO: Implement mechanisms for generating a simulated data stream of valid GNSS positions**

**TODO: Performance over BLE seem to be poor.**