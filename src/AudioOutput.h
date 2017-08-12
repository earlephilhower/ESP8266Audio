/*
  AudioOutput
  Base class of an audio output player
  
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

#ifndef _AUDIOOUTPUT_H
#define _AUDIOOUTPUT_H

#include <Arduino.h>

class AudioOutput 
{
  public:
    AudioOutput() {};
    virtual ~AudioOutput() {};
    virtual bool SetRate(int hz) { hertz = hz; return true; };
    virtual bool SetBitsPerSample(int bits) { bps = bits; return true; };
    virtual bool SetChannels(int chan) { channels = chan; return true; };
    virtual bool begin() { return false; };
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;
    virtual bool ConsumeSample(int16_t sample[2]) { (void)sample; return false; };
    virtual bool stop() { return false; };

    void MakeSampleStereo16(int16_t sample[2]) {
      // Mono to "stereo" conversion
      if (channels == 1)
        sample[RIGHTCHANNEL] = sample[LEFTCHANNEL];
      if (bps == 8) {
        // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL] = (((int16_t)(sample[LEFTCHANNEL]&0xff)) - 128) << 8;
        sample[RIGHTCHANNEL] = (((int16_t)(sample[RIGHTCHANNEL]&0xff)) - 128) << 8;
      }
    };

  protected:
    int hertz;
    int bps;
    int channels;
};

#endif

