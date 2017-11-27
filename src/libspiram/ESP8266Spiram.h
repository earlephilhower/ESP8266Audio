/***********************************************************************************
 * ESP8266Spiram.h - Library designed to manage SRAM Memory chip 23LC1024 via HSPI *
 * Created by Giancarlo Bacchio - August 15, 2015.                                 *
 * see datasheet at http://ww1.microchip.com/downloads/en/DeviceDoc/25142A.pdf     *
 * Released into the public domain.                                                *
 ***********************************************************************************/

/* 
 In this library, the Ram is connected to HSPI using the following pins:
(see SPI Mode, para3.8 pg.13 of 23LC1024 datasheet)
		pin1	   C/S-->GPIO15
		pin2	  MISO-->GPIO12
		pin3       N/C-->Gnd
		pin4	   Vss-->Gnd
		pin5	  MOSI-->GPIO13
		pin6	   SCK-->GPIO14
		pin7	  HOLD-->V+
		pin8	   Vcc-->V+

***********************************************************************************/


#ifndef ESP8266Spiram_h
#define ESP8266Spiram_h

#include <Arduino.h>
#include <SPI.h>

//set of variables defined as per table2.1 pg6
const uint8_t READ = 0x03;
const uint8_t WRITE = 0x02;
const uint8_t EDIO = 0x3B;
const uint8_t EQIO = 0x38;
const uint8_t RSTIO = 0xFF;
const uint8_t RDMR = 0x05;
const uint8_t WRMR = 0x01;

//Mode Register setting as per para 2.2 pg5
const uint8_t REG_BM = 0x00;
const uint8_t REG_PM = 0x80;
const uint8_t REG_SM = 0x40;

class ESP8266Spiram
{
  public:
    ESP8266Spiram();
    void begin(uint8_t csPin);
    void write(uint32_t addr, uint8_t *buff, int len);
    void read(uint32_t addr, uint8_t *buff, int len);
    void setByteMode(void);
    void setPageMode(void);
    void setSeqMode(void);
    uint8_t getMode();
  private:
    void beginTrans_(void);
    void endTrans_(void);
    void writeReg_(uint8_t reg);
    void reset_();
    int Cs;
    uint8_t readReg_(void);
    uint8_t transfer8(uint8_t data);
  	uint16_t transfer16(uint16_t data);
  	uint32_t transfer32(uint32_t data);
};

extern ESP8266Spiram Spiram;

#endif
