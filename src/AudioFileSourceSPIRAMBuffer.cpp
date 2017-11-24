/*
  AudioFileSourceBuffer
  Double-buffered file source using system RAM
  
  Copyright (C) 2017  Earle F. Philhower, III

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

AudioFileSourceSPIRAMBuffer::AudioFileSourceSPIRAMBuffer(AudioFileSource *source, uint32_t buffSizeBytes)
{
  Spiram.begin();
  Spiram.setSeqMode();
  buffSize = 2048; //Size of temp buffer
  ramSize = buffSizeBytes;
  buffer = (uint8_t*)malloc(sizeof(uint8_t) * buffSize);
  writePtr = 0;
  readPtr = 0;
  src = source;
  length = 0;
  filled = false;
  bytesAvailable = 0;
  Serial.printf("RAM buffer size %u\n", ramSize);
}

AudioFileSourceSPIRAMBuffer::~AudioFileSourceSPIRAMBuffer()
{
  free(buffer);
  buffer = NULL;
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
  free(buffer);
  buffer = NULL;
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
  //if (!buffer) return src->read(data, len);

  uint32_t bytes = 0;
  if (!filled) {
    writePtr = readPtr = 0;
    uint16_t toRead=buffSize;
    // Fill up completely before returning any data at all
    Serial.println("Buffering...");
    while (bytesAvailable!=ramSize) {
      length = src->read(buffer, toRead);
      if(length>0) {
        Spiram.write(writePtr, buffer, length);
        bytesAvailable+=length;
        writePtr = bytesAvailable % ramSize;
        if ((ramSize-bytesAvailable)<toRead) {
          toRead=ramSize-bytesAvailable;
        }
      }
    }
    writePtr = bytesAvailable % ramSize;
    filled = true;
    Serial.println("Filling Done !");
  }

  uint8_t *ptr = reinterpret_cast<uint8_t*>(data);
  uint32_t toReadFromBuffer = (len < bytesAvailable) ? len : bytesAvailable;
  if (toReadFromBuffer>0) {
     // Pull from buffer until we've got none left or we've satisfied the request
    Spiram.read(readPtr, ptr, toReadFromBuffer);
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

bool AudioFileSourceSPIRAMBuffer::bufferFill()
{
  if (!filled || bytesAvailable==ramSize) return false; //Make sure the buffer is pre-filled before
  // Now trying to refill SPI RAM Buffer
  uint16_t toReadFromSrc = buffSize;
  if ((ramSize - bytesAvailable)<buffSize) {
	toReadFromSrc = ramSize - bytesAvailable;
  }
  uint16_t cnt = src->readNonBlock(buffer, toReadFromSrc);
  if (cnt) {
    Spiram.write(writePtr, buffer, cnt);
    bytesAvailable+=cnt;
    writePtr = (writePtr + cnt) % ramSize;
  }
  return true;
}
