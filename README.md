# ENBLE

ENBLE is the **EN**vironmental sensor which measures temperature, humidity, and air pressure and sends them over **BLE**.
Because this consumes little current, it can work several years.

![](doc/enble_function.svg)

Using BridgeServer and [ThingsBooard](https://thingsboard.io/) which are runnning on RaspberryPi3, 
you can collect data from some ENBLEs and view measurement data via Web Browser.

![](doc/enble_system.svg)


## Contents of this repository

This repository consists of two components.

### 1. Sensor 

This is about PCB design and firmware for a ENBLE device. 
The details is in README.md in the following.

[**ENBLE/sensor**](sensor/)

### 2. Bridge Server

This is a bridge between ENBLE and [**ThingsBooard**](https://thingsboard.io/) as a sample code to utilize ENBLE. 
This listens BLE advertise packets from ENBLEs and sends data to a ThingsBoard server. 
Using ThingsBoard enables you to collect and visualize measured data. 

[**ENBLE/bridge_server**](bridge_server/)
