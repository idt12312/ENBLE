# ENBLE sensor 


## BLE Services

| Name          | Type           | Permissions | UUID                                 | DataType |
|---------------|----------------|-------------|--------------------------------------|----------|
| ENBLE Service | Service        | Read        | bff20001-378e-4955-89d6-25948b941062 |          |
| DeviceID      | Characteristic | Read, Write | bff20011-378e-4955-89d6-25948b941062 | uint16   |
|  Period       | Characteristic | Read, Write | bff20012-378e-4955-89d6-25948b941062 | uint16   |
| Battery       | Characteristic | Read        | bff20021-378e-4955-89d6-25948b941062 | uint16   |
| Temperature   | Characteristic | Read        | bff20022-378e-4955-89d6-25948b941062 | uint16   |
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

### Humidity
This characteristic indicates humidity in % **multiplied by 10**. 

### AirPressure
This characteristic indicates air pressure in hPa. 



## Set up for build
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
    
