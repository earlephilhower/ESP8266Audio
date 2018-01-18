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
#ifdef ESP32
  #include "driver/i2s.h"
#else
  #include <i2s.h>
#endif
#include "AudioOutputI2SDAC.h"

bool AudioOutputI2SDAC::i2sOn = false;

#ifdef ESP32
//i2s configuration 
int i2s_num = 0; // i2s port number
i2s_config_t i2s_config_dac = {
     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
     .sample_rate = 36000,
     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
     .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
     .dma_buf_count = 8,
     .dma_buf_len = 64,   //Interrupt level 1
     .use_apll = true // Use audio PLL
    };
    
i2s_pin_config_t pin_config_dac = {
    .bck_io_num = 26, //this is BCK pin
    .ws_io_num = 25, // this is LRCK pin
    .data_out_num = 22, // this is DATA output pin
    .data_in_num = -1   //Not used
};
#endif

AudioOutputI2SDAC::AudioOutputI2SDAC()
{
#ifdef ESP32
  if (!i2sOn) {
   //initialize i2s with configurations above
   i2s_driver_install((i2s_port_t)i2s_num, &i2s_config_dac, 0, NULL);
   i2s_set_pin((i2s_port_t)i2s_num, &pin_config_dac);
  } 
#else
  if (!i2sOn) i2s_begin();
#endif
  i2sOn = true;
  mono = false;
  SetGain(1.0);
}

AudioOutputI2SDAC::~AudioOutputI2SDAC()
{
#ifdef ESP32
  if (!i2sOn) {
    i2s_driver_uninstall((i2s_port_t)i2s_num); //stop & destroy i2s driver
  }	
#else
  if (i2sOn) i2s_end();
#endif
  i2sOn = false;
}

bool AudioOutputI2SDAC::SetRate(int hz)
{
  // TODO - have a list of allowable rates from constructor, check them
  this->hertz = hz;
#ifdef ESP32
  i2s_set_sample_rates((i2s_port_t)i2s_num, hz); 
#else
  i2s_set_rate(hz);
#endif
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
  uint32_t s32 = ((Amplify(sample[RIGHTCHANNEL]))<<16) | (Amplify(sample[LEFTCHANNEL]) & 0xffff);
#ifdef ESP32
  return i2s_write_bytes((i2s_port_t)i2s_num, (const char *)&s32, sizeof(uint32_t), 100);
#else
  return i2s_write_sample_nb(s32); // If we can't store it, return false.  OTW true
#endif
}

bool AudioOutputI2SDAC::stop()
{
  // Nothing to do here.  Maybe mute control if DAC doesn't do it for you
  return true;
}


