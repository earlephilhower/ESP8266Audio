/*
  AudioOutputI2SNoDAC
  Audio player using SW delta-sigma to generate "analog" on I2S data
 
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
#include "AudioOutputI2SNoDAC.h"


#ifdef ESP32
i2s_config_t i2s_config = {
     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
     .sample_rate = 44100,
     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, /* the DAC module will only take the 8bits from MSB */
     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
     .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S_MSB,
     .intr_alloc_flags = 0, // default interrupt priority
     .dma_buf_count = 8,
     .dma_buf_len = 64,
     .use_apll = 0
    };
    
i2s_pin_config_t pin_config = {
    .bck_io_num = 26, //this is BCK pin
    .ws_io_num = 25, // this is LRCK pin
    .data_out_num = 22, // this is DATA output pin
    .data_in_num = -1   //Not used
};
#endif

bool AudioOutputI2SNoDAC::i2sOn = false;

AudioOutputI2SNoDAC::AudioOutputI2SNoDAC()
{
#ifdef ESP32
  if (!i2sOn) {
   //initialize i2s with configurations above
   
   i2s_driver_install((i2s_port_t)0, &i2s_config, 0, NULL);
   i2s_set_pin((i2s_port_t)0, &pin_config);
  } 
#else
  if (!i2sOn) i2s_begin();
#endif
  i2sOn = true;
  hertz = 44100;
  oversample = 32;
  lastSamp = 0;
  cumErr = 0;
  SetGain(1.0);
}

AudioOutputI2SNoDAC::~AudioOutputI2SNoDAC()
{
#ifdef ESP32
  if (!i2sOn) {
    i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver
  }	
#else
  if (i2sOn) i2s_end();
#endif
  i2sOn = false;
}

bool AudioOutputI2SNoDAC::SetRate(int hz)
{
  // TODO - what is the max hz we can request on I2S?
  this->hertz = hz;
#ifdef ESP32
  i2s_set_sample_rates((i2s_port_t)0, hz * (oversample / 32)); 
#else
  i2s_set_rate(hz * (oversample / 32));
#endif
  return true;
}

bool AudioOutputI2SNoDAC::SetBitsPerSample(int bits)
{
  if ( (bits != 16) && (bits != 8) ) return false;
  this->bps = bits;
  return true;
}

bool AudioOutputI2SNoDAC::SetChannels(int channels)
{
  if ( (channels < 1) || (channels > 2) ) return false;
  this->channels = channels;
  return true;
}

bool AudioOutputI2SNoDAC::SetOversampling(int os) {
  if (os % 32) return false;  // Only Nx32 oversampling supported
  if (os > 256) return false; // Don't be silly now!
  if (os < 32) return false;  // Nothing under 32 allowed

  oversample = os;
  return SetRate(hertz);
}

bool AudioOutputI2SNoDAC::begin()
{
  // Nothing to do here, i2s will start once data comes in
  return true;
}

void AudioOutputI2SNoDAC::DeltaSigma(int16_t sample[2], uint32_t dsBuff[4])
{
//  // Not shift 8 because addition takes care of one mult x 2
//  fixed24p8_t newSamp = ( ((int32_t)sample[0]) + ((int32_t)sample[1]) ) << 7;
  int32_t sum = (((int32_t)sample[0]) + ((int32_t)sample[1])) >> 1;
  fixed24p8_t newSamp = ( (int32_t)Amplify(sum) ) << 8;

  int oversample32 = oversample / 32;
  // How much the comparison signal changes each oversample step
  fixed24p8_t diffPerStep = (newSamp - lastSamp) >> (4 + oversample32);

  // Don't need lastSamp anymore, store this one for next round
  lastSamp = newSamp;

  for (int j = 0; j < oversample32; j++) {
    uint32_t bits = 0; // The bits we convert the sample into, MSB to go on the wire first
    
    for (int i = 32; i > 0; i--) {
      bits = bits << 1;
      if (cumErr < 0) {
        bits |= 1;
        cumErr += fixedPosValue - newSamp;
      } else {
        // Bits[0] = 0 handled already by left shift
        cumErr -= fixedPosValue + newSamp;
      }
      newSamp += diffPerStep; // Move the reference signal towards destination
    }
    dsBuff[j] = bits;
  }
}

#ifdef ESP32
/* write sample data to I2S */
int i2s_write_sample_nb(uint8_t sample){
  return i2s_write_bytes((i2s_port_t)0, (const char *)&sample, sizeof(uint8_t), 100);
}

// how to do this blocking??
int i2s_write_sample(uint8_t sample){
  return i2s_write_bytes((i2s_port_t)0, (const char *)&sample, sizeof(uint8_t), 100);
}

#endif

bool AudioOutputI2SNoDAC::ConsumeSample(int16_t sample[2])
{
  MakeSampleStereo16( sample );

  // Make delta-sigma filled buffer
  uint32_t dsBuff[4];
  DeltaSigma(sample, dsBuff);
  
  // Either send complete pulse stream or nothing
  if (!i2s_write_sample_nb(dsBuff[0])) return false; // No room at the inn
  // At this point we've sent in first of possibly 4 32-bits, need to send
  // remaining ones even if they block.
  for (int i = 32; i < oversample; i+=32)
    i2s_write_sample( dsBuff[i / 32]);
  return true;
}

bool AudioOutputI2SNoDAC::stop()
{
  // Nothing to do here.  Maybe mute control if DAC doesn't do it for you
  return true;
}


