/*
  AudioGenerator
  Base class of an audio output generator
  
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

#ifndef _AUDIOGENERATOR_H
#define _AUDIOGENERATOR_H

#include <Arduino.h>
#include "AudioFileSource.h"
#include "AudioOutput.h"

class AudioGenerator
{
  public:
    AudioGenerator() {};
    virtual ~AudioGenerator() {};
    virtual bool begin(AudioFileSource *source, AudioOutput *output) { (void)source; (void)output; return false; };
    virtual bool loop() { return false; };
    virtual bool stop() { return false; };
    virtual bool isRunning() { return false;};

  protected:
    bool running;
    AudioFileSource *file;
    AudioOutput *output;
};


#endif

