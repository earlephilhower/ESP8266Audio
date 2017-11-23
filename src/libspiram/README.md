# Arduino IDE ESP8266 - library for SPI Serial SRam 23A1024/23LC1024 by Microchip Technology Inc.

This Library provides the interface for a 1Mbit SRAM chip
with ESP8266 through SPI connection.
Microchip 23A1024/23LC1024 - [download datasheet here](http://ww1.microchip.com/downloads/en/DeviceDoc/20005142C.pdf)

Application written by [Giancarlo Bacchio](bacchio.giancarlo@gmail.com)


## Getting the code

You can install SRam Library for your ESP8266 downloading the code that is hosted at https://github.com/Gianbacchio/ESP8266_Spiram.

Selecting "Watch" box in the GitHub repository, you will be informed about the latest development updates of this library. 


## Installing the code

The downloaded code in *.zip format can be included as a new library into the IDE, selecting the menù:

     Sketch / include Library / Add .Zip library	


## Usage

The Hardware connection is done using the standard HSPI interface of the ESP8266.

The chip is connected as per para 3.8 "SPI/SDI and SQI Pin designation" (see datasheet)

Power supply is 3,3Vcc.

Pin on RAM chip | functionality | Pin on ESP8266
----------------|---------------|----------------
   pin1         |  C/S          |  GPIO15
   pin2         |  MISO         |  GPIO12
   pin3         |  N/C          |  Gnd
   pin4         |  Vss          |  Gnd
   pin5         |  MOSI         |  GPIO13
   pin6         |  SCK          |  GPIO14
   pin7         |  HOLD         |  V+
   pin8         |  Vcc          |  V+


## Examples

- ESP_spiram.ino : a basic example to understand how the library works. You can write and read a Byte in a given location of the SRAM
 

## License

You may copy, distribute and modify the software provided that modifications are described and licensed for free under [LGPL-3](http://www.gnu.org/licenses/lgpl-3.0.html). Derivatives works (including modifications or anything statically linked to the library) can only be redistributed under [LGPL-3](http://www.gnu.org/licenses/lgpl-3.0.html), but applications that use the library don't have to be.


