/*
    AudioOutputSPIFFSWAV
    Writes a WAV file to the SPIFFS filesystem

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

#ifndef _AUDIOOUTPUTSPIFFSWAV_H
#define _AUDIOOUTPUTSPIFFSWAV_H

#if !defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>
#include <FS.h>

#include "AudioOutput.h"

class AudioOutputSPIFFSWAV : public AudioOutput {
public:
    AudioOutputSPIFFSWAV() {
        filename = NULL;
    };
    ~AudioOutputSPIFFSWAV() {
        free(filename);
    };
    virtual bool begin() override;
    virtual bool ConsumeSample(int16_t sample[2]) override;
    virtual bool stop() override;
    void SetFilename(const char *name);

private:
    File f;
    char *filename;
};

#endif

#endif
