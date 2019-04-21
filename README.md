# ENBLE

ENBLE is the **EN**vironmental sensor which measures temperature, humidity, and air pressure and sends them over **BLE**.
Because it consumes little current, it can work several years.

![](doc/enble_function.svg)

Using bridge server and [ThingsBooard](https://thingsboard.io/) which are runnning on Raspberry Pi3, 
you can collect data from some ENBLEs and view measurement data via web browser.

![](doc/enble_system.svg)


## Contents of this repository

This repository consists of two components.

### 1. Sensor 

[**ENBLE/sensor**](sensor/)

It contains PCB design and firmware for a ENBLE device. 
The details is in README.md.

### 2. Bridge Server

[**ENBLE/bridge_server**](bridge_server/)

It constains a bridge between ENBLE and [**ThingsBooard**](https://thingsboard.io/) as a sample code to utilize ENBLE. 
Bridge server listens BLE advertise packets from ENBLEs and sends data to a ThingsBoard server. 
Using ThingsBoard enables you to collect and visualize measured data. 

