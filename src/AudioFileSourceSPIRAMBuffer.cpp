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
  buffSize = 1024; //Size of temp buffer
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
  if (!buffer) return src->read(data, len);

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
      yield();
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

  if (len) {
    bytes += src->read(ptr, len);
    bytesAvailable = 0;
    filled = false;
  }

return bytes;

//  Serial.println("Read...OK");

/*  uint32_t toReadFromBuffer = (len < length) ? len : length;
  if ( (toReadFromBuffer > 0) && (readPtr >= writePtr) ) {
    uint32_t toReadToEnd = (toReadFromBuffer < (uint32_t)(buffSize - readPtr)) ? toReadFromBuffer : (buffSize - readPtr);
    memcpy(ptr, &buffer[readPtr], toReadToEnd);
    readPtr = (readPtr + toReadToEnd) % buffSize;
    len -= toReadToEnd;
    length -= toReadToEnd;
    ptr += toReadToEnd;
    bytes += toReadToEnd;
    toReadFromBuffer -= toReadToEnd;
  }
  if (toReadFromBuffer > 0) { // We know RP < WP at this point
    memcpy(ptr, &buffer[readPtr], toReadFromBuffer);
    readPtr = (readPtr + toReadFromBuffer) % buffSize;
    len -= toReadFromBuffer;
    length -= toReadFromBuffer;
    ptr += toReadFromBuffer;
    bytes += toReadFromBuffer;
    toReadFromBuffer -= toReadFromBuffer;
  }

  if (len) {
    // Still need more, try direct read from src
    bytes += src->read(ptr, len);
    // We're out of buffered data, need to force a complete refill.  Thanks, @armSeb
    readPtr = 0;
    writePtr = 0;
    length = 0;
    filled = false;
    return bytes; 
  }

  if (length < buffSize) {
    // Now try and opportunistically fill the buffer
    if (readPtr > writePtr) {
        int bytesAvailMid = readPtr - writePtr - 1;
        if (bytesAvailMid > 0) {
          int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailMid);
          length += cnt;
          writePtr = (writePtr + cnt) % buffSize;
        }
      return bytes;
    }

    int bytesAvailEnd = buffSize - writePtr;
    if (bytesAvailEnd > 0) {
      int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailEnd);
      length += cnt;
      writePtr = (writePtr + cnt) % buffSize;
      if (cnt != bytesAvailEnd) return bytes; 
    }
    int bytesAvailStart = readPtr - 1;
    if (bytesAvailStart > 0) {
      int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailStart);
      length += cnt;
      writePtr = (writePtr + cnt) % buffSize;
    }
  }

  return bytes;*/
}

