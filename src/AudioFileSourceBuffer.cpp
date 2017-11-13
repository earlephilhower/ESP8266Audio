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
#include "AudioFileSourceBuffer.h"

AudioFileSourceBuffer::AudioFileSourceBuffer(AudioFileSource *source, int buffSizeBytes)
{
  buffSize = buffSizeBytes;
  buffer = (uint8_t*)malloc(sizeof(uint8_t) * buffSize);
  writePtr = 0;
  readPtr = 0;
  src = source;
  length = 0;
  filled = false;
}

AudioFileSourceBuffer::~AudioFileSourceBuffer()
{
  free(buffer);
  buffer = NULL;
}

bool AudioFileSourceBuffer::seek(int32_t pos, int dir)
{
  // Invalidate
  readPtr = 0;
  writePtr = 0;
  length = 0;
  return src->seek(pos, dir);
}

bool AudioFileSourceBuffer::close()
{
  free(buffer);
  buffer = NULL;
  return src->close();
}

bool AudioFileSourceBuffer::isOpen()
{
  return src->isOpen();
}

uint32_t AudioFileSourceBuffer::getSize()
{
  return src->getSize();
}

uint32_t AudioFileSourceBuffer::getPos()
{
  return src->getPos();
}

uint32_t AudioFileSourceBuffer::read(void *data, uint32_t len)
{
  if (!buffer) return src->read(data, len);

  uint32_t bytes = 0;
  if (!filled) {
    // Fill up completely before returning any data at all
    length = src->read(buffer, buffSize);
    writePtr = length % buffSize;
    filled = true;
  }

  // Naively pull from buffer until we've got none left or we've satisfied the request
  uint8_t *ptr = reinterpret_cast<uint8_t*>(data);
  while (len && length) {
    *ptr = buffer[readPtr];
    ptr++;
    readPtr = (readPtr+1) % buffSize;
    len--;
    bytes++;
    length--;
  }
//  Serial.printf("read %d from buffer, %d avail in buff, %d remaining for request\n", bytes, length, len);

  if (len) {
    // Still need more, try direct read from src
    bytes += src->read(ptr, len);
  }

  if (length < buffSize) {
    // Now try and opportunistically fill the buffer
    if (readPtr > writePtr) {
        int bytesAvailMid = readPtr - writePtr - 1;
        if (bytesAvailMid > 0) {
//Serial.print("at mid: ");
          int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailMid);
//Serial.printf("read %d\n", cnt);
          length += cnt;
          writePtr = (writePtr + cnt) % buffSize;
        }
      return bytes;
    }



    int bytesAvailEnd = buffSize - writePtr;
    if (bytesAvailEnd > 0) {
//Serial.print("at end: ");
      int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailEnd);
//Serial.printf("read %d\n", cnt);
      length += cnt;
      writePtr = (writePtr + cnt) % buffSize;
      if (cnt != bytesAvailEnd) return bytes; 
    }
    int bytesAvailStart = readPtr - 1;
    if (bytesAvailStart > 0) {
//Serial.print("at begin: ");
      int cnt = src->readNonBlock(&buffer[writePtr], bytesAvailStart);
//Serial.printf("read %d\n", cnt);
      length += cnt;
      writePtr = (writePtr + cnt) % buffSize;
    }
  }

  return bytes;
}

