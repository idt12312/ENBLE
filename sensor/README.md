# ENBLE sensor 

## BLE Advertise

| Parameter          | Value                 |
|--------------------|-----------------------|
| Local Name         | "ENBLE"               |
| Advertise Interval | 2000ms                |
| Manufacturer data  | 10 bytes binary data  |

Manufacturer data has DeviceID and measurement results of sensors.  
Data format is as shown below. 

| Position | Contents    | DataType |
|----------|-------------|----------|
| byte 0-1 | DeviceID    | uint16   |
| byte 2-3 | Battery     | uint16   |
| byte 4-5 | Temperature | int16    |
| byte 6-7 | Humidity    | uint16   |
| byte 8-9 | Pressure    | uint16   |

Each data is contained in **little endian 16bit integer**.  
Means of above data are same as ones in bellow characteristics.


## BLE Services

| Name          | Type           | Permissions | UUID                                 | DataType |
|---------------|----------------|-------------|--------------------------------------|----------|
| ENBLE Service | Service        | Read        | bff20001-378e-4955-89d6-25948b941062 |          |
| DeviceID      | Characteristic | Read, Write | bff20011-378e-4955-89d6-25948b941062 | uint16   |
|  Period       | Characteristic | Read, Write | bff20012-378e-4955-89d6-25948b941062 | uint16   |
| Battery       | Characteristic | Read        | bff20021-378e-4955-89d6-25948b941062 | uint16   |
| Temperature   | Characteristic | Read        | bff20022-378e-4955-89d6-25948b941062 | int16    |
| Humidity      | Characteristic | Read        | bff20023-378e-4955-89d6-25948b941062 | uint16   |
| Pressure      | Characteristic | Read        | bff20024-378e-4955-89d6-25948b941062 | uint16   |



### DeviceID
This characteristic indicates a unique number as 16bit unsigned integer. 
The value of this characteristic is stored in nonvolatile memory. 

### Period
This characteristic indicates a sample period of sensors. 
All sensors (battery, temperature, humidity and pressure) sample at the same period. 
This period is 16 bit unsigned integer in seconds. 
The value of this characteristic is stored in nonvolatile memory. 

### Battery
This characteristic indicates battery voltage in % **multiplied by 100**. 

### Temperature
This characteristic indicates temperature in ℃ **multiplied by 10**. 
A negative value is represented in two's complement number. 

### Humidity
This characteristic indicates humidity in % **multiplied by 10**. 

### Pressure
This characteristic indicates air pressure in hPa. 



## Setup for build
1. Download Nordic SDK ver 12.3.0
2. Place Nordic SDK in firmware/lib  
ex) The structure of directory must be is following.   
    firmware/lib/  
        └──nRF5_SDK_12.3.0_d7731ad/ (this directory name can be anything you like)  
        .       ├──components/  
        .       ├──external/  
        .       ├── ......  

3. Modify NORDIC_SDK_PATH parameter in firmware/Makefile
4. Modify toolchain configuration in 
    * for Linux or OSX user : $(NORDIC_SDK_PATH)/components/toolchain/gcc/Makefile.posix
    * for Windows user : $(NORDIC_SDK_PATH)/components/toolchain/gcc/Makefile.windows
    
