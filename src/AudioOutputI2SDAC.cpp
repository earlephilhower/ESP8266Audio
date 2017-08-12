/*
  AudioOutputI2SDAC
  Audio player for an I2S connected DAC, 16bps
  
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
#include <i2s.h>
#include "AudioOutputI2SDAC.h"

bool AudioOutputI2SDAC::i2sOn = false;

AudioOutputI2SDAC::AudioOutputI2SDAC()
{
  if (!i2sOn) i2s_begin();
  i2sOn = true;
}

AudioOutputI2SDAC::~AudioOutputI2SDAC()
{
  if (i2sOn) i2s_end();
  i2sOn = false;
}

bool AudioOutputI2SDAC::SetRate(int hz)
{
  // TODO - have a list of allowable rates from constructor, check them
  this->hertz = hz;
  i2s_set_rate(hz);
  return true;
}

bool AudioOutputI2SDAC::SetBitsPerSample(int bits)
{
  if ( (bits != 16) && (bits != 8) ) return false;
  this->bps = bits;
  return true;
}

bool AudioOutputI2SDAC::SetChannels(int channels)
{
  if ( (channels < 1) || (channels > 2) ) return false;
  this->channels = channels;
  return true;
}

bool AudioOutputI2SDAC::SetOutputModeMono(bool mono)
{
  this->mono = mono;
  return true;
}

bool AudioOutputI2SDAC::begin()
{
  // Nothing to do here, i2s will start once data comes in
  return true;
}

bool AudioOutputI2SDAC::ConsumeSample(int16_t sample[2])
{
  MakeSampleStereo16( sample );

  if (this->mono) {
    // Average the two samples and overwrite
    uint32_t ttl = sample[LEFTCHANNEL] + sample[RIGHTCHANNEL];
    sample[LEFTCHANNEL] = sample[RIGHTCHANNEL] = (ttl>>1) & 0xffff;
  }
  uint32_t s32 = ((sample[RIGHTCHANNEL])<<16) | (sample[LEFTCHANNEL] & 0xffff);
  return i2s_write_sample_nb(s32); // If we can't store it, return false.  OTW true
}

bool AudioOutputI2SDAC::stop()
{
  // Nothing to do here.  Maybe mute control if DAC doesn't do it for you
  return true;
}


