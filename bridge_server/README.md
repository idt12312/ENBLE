# ENBLE Bridge Server

This program is a bridge between ENBLE and [**ThingsBooard**](https://thingsboard.io/) as a sample code to utilize ENBLE. 
It listens BLE advertise packets from ENBLEs and sends data to a ThingsBoard server. 
Using ThingsBoard enables you to collect and visualize measured data. 

![](../doc/enble_system.svg)


## How to use
I tested this program on RaspberryPi3.
It may works well on the device which can execute python and has a Bluetooth interface.

Before running bridge_server.py, the following softwares and python packages need to install.

* Python3
* bluepy
* requests
* ThingsBoard

ThingsBoard is not necessary to run on the same device where bridge_server.py runs.

To run this program, you have to do the following.

1. Rename 'config.json.example' 'config.json'.
2. Edit config.json as your environment.
3. Run bridge_server.py.

If some error like the following occures,

```
bluepy.btle.BTLEManagementError: Failed to execute management command 'le on' (code: 20, error: Permission Denied)
```

execute the following.

```
setcap 'cap_net_raw,cap_net_admin+eip' {path_to_python_packages}/site-packages/bluepy/bluepy-helper
```
