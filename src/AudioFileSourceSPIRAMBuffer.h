/*
  AudioFileSourceSPIRAMBuffer
  Buffered file source in external SPI RAM

  Copyright (C) 2017  Sebastien Decourriere
  Based on AudioFileSourceBuffer class from Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AUDIOFILESOURCESPIRAMBUFFER_H
#define _AUDIOFILESOURCESPIRAMBUFFER_H

#include "AudioFileSource.h"
#include <SPI.h>
#include <ESP8266Spiram.h> // https://github.com/Gianbacchio/ESP8266_Spiram

// #define SPIBUF_DEBUG

class AudioFileSourceSPIRAMBuffer : public AudioFileSource
{
  public:
    AudioFileSourceSPIRAMBuffer(AudioFileSource *in, uint8_t csPin, uint32_t bufferBytes);
    virtual ~AudioFileSourceSPIRAMBuffer() override;

    virtual uint32_t read(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;
    virtual bool loop() override;

  private:
    virtual void fill();

  private:
    AudioFileSource *src;
    ESP8266Spiram *Spiram;
    uint32_t ramSize;
    uint32_t writePtr;
    uint32_t readPtr;
    uint16_t length;
    uint32_t bytesAvailable;
    bool filled;
};


#endif

