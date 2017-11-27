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

************************************************************************************/



#include "ESP8266Spiram.h"


// setup of C/S line as per HSPI default
ESP8266Spiram::ESP8266Spiram() {
      Cs=15; // default value
}


// Activate the library setting up SequentialMode of operation (see 2.2 pg.5)
void ESP8266Spiram::begin(uint8_t csPin)	{

      if (csPin<16) Cs=csPin;

      SPI.begin();
      pinMode (Cs, OUTPUT);
      digitalWrite(Cs, HIGH);

      digitalWrite(Cs, HIGH);
      delay(50);
      digitalWrite(Cs, LOW);
      delay(50);
      digitalWrite(Cs, HIGH);
      setSeqMode();

}

/***********************************************
 * this function Read SRAM                     * 
 *   - addr: address to start read             *
 *   - *buff: list where read bytes are copied *
 *   - len: amount of bytes to read            *
 * (see sequence in figure 2-1)                *
 ***********************************************/

void ESP8266Spiram::read(uint32_t addr, uint8_t *buff, int len) {
        uint32_t instr; 
        int i=0;
        instr=(READ<<24)|(addr++&0x00ffffff);
        beginTrans_();
        transfer32(instr);   // code to set Read mode in the SRAM
        while (len--) {
          buff[i++]=transfer8(0);
        }
        endTrans_();
}


/**************************************
 * this function Write in the SRAM    * 
 *   - addr: address to start write   *
 *   - *buff: list of bytes to write  *
 *   - len: amount of bytes to write  *
 * (see sequence in figure 2-2)       *
 **************************************/

void ESP8266Spiram::write(uint32_t addr, uint8_t *buff, int len) {
        uint32_t instr;
        int i=0;
        instr=(WRITE<<24)|(addr++&0x00ffffff);
        beginTrans_();
        transfer32(instr);   // code to set Read mode in the SRAM
        while (len--) {
          transfer8(buff[i++]);
	}	
        endTrans_();
}


/*************************************************
 * this function Read the configuration register *
 *************************************************/

uint8_t ESP8266Spiram::readReg_(void) {
       beginTrans_();
       transfer8(RDMR);
       uint8_t reg=transfer8(0);
       endTrans_();
       return reg;
}

/**************************************************
 * this function Write the configuration register *
 **************************************************/

void ESP8266Spiram::writeReg_(uint8_t reg) {
       beginTrans_();
       transfer8(WRMR);
       transfer8(reg);
       endTrans_();
}

/********************************
 * this function reset the SRAM *
 ********************************/

void ESP8266Spiram::reset_(void) {  
       beginTrans_();
       SPI.transfer(RSTIO);
       endTrans_();
}

uint8_t ESP8266Spiram::transfer8(uint8_t data){
 /* FOR DEBUG:
  Serial.print("sending: ");
  Serial.println(data,BIN);
  data = SPI.transfer(data);
  Serial.print("receving: ");
  Serial.println(data,BIN);
  return data;*/
  return SPI.transfer(data);
}

uint16_t ESP8266Spiram::transfer16(uint16_t data){
        union {
            uint16_t val;
            struct {
                    uint8_t lsb;
                    uint8_t msb;
            };
        } in, out;
        in.val = data;

        out.msb = transfer8(in.msb);
        out.lsb = transfer8(in.lsb);

        return out.val;
}

uint32_t ESP8266Spiram::transfer32(uint32_t data){
        union {
            uint32_t val;
            struct {
                    uint16_t lsb;
                    uint16_t msb;
            };
        } in, out;
        in.val = data;

        out.msb = transfer16(in.msb);
        out.lsb = transfer16(in.lsb);

        return out.val;
}


void ESP8266Spiram::setByteMode(void){
        writeReg_(REG_BM);
}

void ESP8266Spiram::setPageMode(void){
        writeReg_(REG_PM);
}

void ESP8266Spiram::setSeqMode(void){
        writeReg_(REG_SM);
}

uint8_t ESP8266Spiram::getMode(void){
        return readReg_();
}

void ESP8266Spiram::beginTrans_(void){
        SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(Cs, LOW);
}

void ESP8266Spiram::endTrans_(void){
        digitalWrite(Cs, HIGH);
        SPI.endTransaction();  
}

