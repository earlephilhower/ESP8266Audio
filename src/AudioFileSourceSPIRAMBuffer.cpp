/*
  AudioFileSourceSPIRAMBuffer
  Buffered file source in external SPI RAM

  Copyright (C) 2017  Sebastien Decourriere
  Based on AudioFileSourceBuffer class from Earle F. Philhower, III

  Copyright (C) 2020  Earle F. Philhower, III
  Rewritten for speed and functionality

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
	ram.begin(5);
	ramSize = buffSizeBytes;
	writePtr = 0;
	readPtr = 0;
	filled = false;
	src = source;
	audioLogger->printf_P(PSTR("SPI RAM buffer size: %u Bytes\n"), ramSize);
}

AudioFileSourceSPIRAMBuffer::~AudioFileSourceSPIRAMBuffer()
{
	ram.end();
}

bool AudioFileSourceSPIRAMBuffer::seek(int32_t pos, int dir)
{
	// Invalidate
	readPtr = 0;
	writePtr = 0;
	filled = false;
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
		audioLogger->printf_P(PSTR("Filling buffer...\n"));
		uint8_t buffer[256];
		writePtr = 0;
		readPtr = 0;
		// Fill up completely before returning any data at all
		do {
			int toRead = std::min(ramSize - (writePtr - readPtr), sizeof(buffer));
			int length = src->read(buffer, toRead);
			if (length > 0) {
				if (writePtr==0) { for (int i=0; i<256; i++) Serial.printf("%02x ", buffer[i]); Serial.printf("\n"); }
				audioLogger->printf_P(PSTR("Preread %d at %d...\n"), length, writePtr);
#ifdef FAKERAM
				for (size_t i=0; i<length; i++) fakeRAM[(i+writePtr)%ramSize] = buffer[i];
#else
				ram.writeBytes(writePtr % ramSize, buffer, length);
#endif
				writePtr += length;
			} else if (length == 0 ) {
				delay(10); // No data, so just hang out a bit...
			} else {
				// EOF, break out of read loop
				break;
			}
		} while ((writePtr - readPtr) < ramSize);
		filled = true;
		audioLogger->printf_P(PSTR("Filling completed...\n"));
	}

//  audioLogger->printf("Buffer: %u%\n", bytesAvailable*100/ramSize);

	// Read up to the entire buffer from RAM
	uint32_t toReadFromBuffer = std::min(len, writePtr - readPtr);
	uint8_t *ptr = reinterpret_cast<uint8_t*>(data);
	if (toReadFromBuffer > 0) {
//		audioLogger->printf_P(PSTR("Avail: %u, reading %u...\n"), writePtr - readPtr, toReadFromBuffer);
#ifdef FAKERAM
		for (size_t i=0; i<toReadFromBuffer; i++) ptr[i] = fakeRAM[(i+readPtr)%ramSize];
#else
		ram.readBytes(readPtr % ramSize, ptr, toReadFromBuffer);
#endif
		if (readPtr==0) { for (int i=0; i<256; i++) Serial.printf("%02x ", ptr[i]); Serial.printf("\n"); }
		readPtr += toReadFromBuffer;
		ptr += toReadFromBuffer;
		bytes += toReadFromBuffer;
		len -= toReadFromBuffer;
	}

  // If len>0 there is no data left in buffer and we try to read more directly from source.
  // Then, we trigger a complete buffer refill
  if (len) {
    bytes += src->read(data, len);
    filled = false;
  }
  return bytes;
}

void AudioFileSourceSPIRAMBuffer::fill()
{
	// Make sure the buffer is pre-filled before make partial fill.
	if (!filled) return;

	uint8_t buffer[128];
	if ((ramSize - (writePtr - readPtr)) < sizeof(buffer)) {
		return;
	}

	// Now trying to refill SPI RAM Buffer
	// Make sure there is at least buffer size free in RAM
	int cnt = src->readNonBlock(buffer, sizeof(buffer));
	if (cnt) {
#ifdef FAKERAM
		for (size_t i=0; i<cnt; i++) fakeRAM[(i+writePtr)%ramSize] = buffer[i];
#else
		ram.writeBytes(writePtr % ramSize, buffer, cnt);
#endif
		writePtr += cnt;
	}
	return;
}

bool AudioFileSourceSPIRAMBuffer::loop()
{
	static uint32_t last = 0;
	if (!src->loop()) return false;
	fill();
	if ((ESP.getCycleCount() - last) > microsecondsToClockCycles(10000000)) {
		last = ESP.getCycleCount();
		audioLogger->printf_P(PSTR("Buffering: %d%%  (%d/%d)\n"), ((writePtr - readPtr) * 100)/ramSize, writePtr-readPtr, ramSize);
	}
	return true;
}
