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

#include <Arduino.h>
#include "AudioFileSourceSPIRAMBuffer.h"

#pragma GCC optimize ("O3")

AudioFileSourceSPIRAMBuffer::AudioFileSourceSPIRAMBuffer(AudioFileSource *source, uint8_t csPin, uint32_t buffSizeBytes)
{
  Spiram = new ESP8266Spiram(csPin, 40e6);
  Spiram->begin();
  Spiram->setSeqMode();
  ramSize = buffSizeBytes;
  writePtr = 0;
  readPtr = 0;
  src = source;
  length = 0;
  filled = false;
  bytesAvailable = 0;
  audioLogger->printf_P(PSTR("SPI RAM buffer size: %u Bytes\n"), ramSize);
}

AudioFileSourceSPIRAMBuffer::~AudioFileSourceSPIRAMBuffer()
{
}

bool AudioFileSourceSPIRAMBuffer::seek(int32_t pos, int dir)
{
  // Invalidate
  readPtr = 0;
  writePtr = 0;
  length = 0;
  return src->seek(pos, dir);
}

bool AudioFileSourceSPIRAMBuffer::close()
{
  return src->close();
}

bool AudioFileSourceSPIRAMBuffer::isOpen()
{
  return src->isOpen();
}

uint32_t AudioFileSourceSPIRAMBuffer::getSize()
{
  return src->getSize();
}

uint32_t AudioFileSourceSPIRAMBuffer::getPos()
{
  return src->getPos();
}

uint32_t AudioFileSourceSPIRAMBuffer::read(void *data, uint32_t len)
{
  uint32_t bytes = 0;
  // Check if the buffer isn't empty, otherwise we try to fill completely
  if (!filled) {
    uint8_t buffer[256];
    writePtr = readPtr = 0;
    uint16_t toRead = sizeof(buffer);
    // Fill up completely before returning any data at all
    audioLogger->printf_P(PSTR("Buffering...\n"));
    while (bytesAvailable!=ramSize) {
      length = src->read(buffer, toRead);
      if(length>0) {
        Spiram->write(writePtr, buffer, length);
        bytesAvailable+=length;
        writePtr = bytesAvailable % ramSize;
        if ((ramSize-bytesAvailable)<toRead) {
          toRead=ramSize-bytesAvailable;
        }
      } else {
        // EOF, break out of read loop
        break;
      }
    }
    writePtr = bytesAvailable % ramSize;
    filled = true;
    audioLogger->printf_P(PSTR("Filling Done !\n"));
  }

//  audioLogger->printf("Buffer: %u%\n", bytesAvailable*100/ramSize);

  uint8_t *ptr = reinterpret_cast<uint8_t*>(data);
  uint32_t toReadFromBuffer = (len < bytesAvailable) ? len : bytesAvailable;
  if (toReadFromBuffer>0) {
     // Pull from buffer until we've got none left or we've satisfied the request
    Spiram->read(readPtr, ptr, toReadFromBuffer);
    readPtr = (readPtr+toReadFromBuffer) % ramSize;
    bytes = toReadFromBuffer;
    bytesAvailable-=toReadFromBuffer;
    len-=toReadFromBuffer;
    ptr += toReadFromBuffer;
  }

  // If len>O there is no data left in buffer and we try to read more directly from source.
  // Then, we trigger a complete buffer refill
  if (len) {
    bytes += src->read(ptr, len);
    bytesAvailable = 0;
    filled = false;
  }
  return bytes;
}

void AudioFileSourceSPIRAMBuffer::fill()
{
  // Make sure the buffer is pre-filled before make partial fill.
  if (!filled) return;

  // Now trying to refill SPI RAM Buffer
  uint8_t buffer[128];
  // Make sure there is at least buffer size free in RAM
  if ((ramSize - bytesAvailable)<sizeof(buffer)) {
	return;
  }
  uint16_t cnt = src->readNonBlock(buffer, sizeof(buffer));
  if (cnt) {
    Spiram->write(writePtr, buffer, cnt);
    bytesAvailable+=cnt;
    writePtr = (writePtr + cnt) % ramSize;
#ifdef SPIBUF_DEBUG
    audioLogger->printf_P(PSTR("SockRead: %u | RamAvail: %u\n"), cnt, bytesAvailable);
#endif
  }
  return;
}

bool AudioFileSourceSPIRAMBuffer::loop()
{
  if (!src->loop()) return false;
  fill();
  return true;
}
