/*
  AudioOutputI2SNoDACS3
  Base class for I2S interface port
  
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
#include "AudioOutputI2SNoDACS3.h"

#if CONFIG_IDF_TARGET_ESP32S3
AudioOutputI2SNoDACS3::AudioOutputI2SNoDACS3(int doutPin, int dummyPin, int port, int dma_buf_count)
{
  this->doutPin = (gpio_num_t)doutPin;
  this->dummyPin = (gpio_num_t)dummyPin;
  
  this->portNo = port;
  this->dma_buf_count = dma_buf_count;

  // Set initial I2S state
  i2sOn = false;

  // Set defaults
  mono = false;
  bps = 16;
  channels = 2;
  hertz = 44100;
  SetGain(1.0);
}

AudioOutputI2SNoDACS3::~AudioOutputI2SNoDACS3()
{
  stop();
}

bool AudioOutputI2SNoDACS3::SetRate(int hz)
{
  // TODO - have a list of allowable rates from constructor, check them
  this->hertz = hz;
  if (i2sOn)
  {
    i2s_set_sample_rates((i2s_port_t)portNo, AdjustI2SRate(hz));
  }
  return true;
}

bool AudioOutputI2SNoDACS3::SetBitsPerSample(int bits)
{
  if ( (bits != 16) && (bits != 8) ) return false;
  this->bps = bits;
  return true;
}

bool AudioOutputI2SNoDACS3::SetChannels(int channels)
{
  if ( (channels < 1) || (channels > 2) ) return false;
  this->channels = channels;
  return true;
}

bool AudioOutputI2SNoDACS3::SetOutputModeMono(bool mono)
{
  this->mono = mono;
  return true;
}

bool AudioOutputI2SNoDACS3::begin()
{
  if (!i2sOn)
  {
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_PDM),
      .sample_rate = 30000, // initial value ?????????
      .bits_per_sample = i2s_bits_per_sample_t(bps),
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
      .dma_buf_count = dma_buf_count,
      .dma_buf_len = 128,
      .use_apll = true,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    audioLogger->printf("+%d %p\n", portNo, &i2s_config);
    if (i2s_driver_install((i2s_port_t)portNo, &i2s_config, 0, NULL) != ESP_OK)
    {
      audioLogger->println(F("ERROR: Unable to install I2S drives\n"));
      return false;
    }

    // Set pinout
    i2s_pin_config_t i2s_pinout = {
      .bck_io_num = I2S_PIN_NO_CHANGE, // no bck
      .ws_io_num = dummyPin, // dummy pin for LR clock
      .data_out_num = doutPin, // PDM data out
      .data_in_num = I2S_PIN_NO_CHANGE // no input
    };

    if (i2s_set_pin((i2s_port_t)portNo, &i2s_pinout) != ESP_OK)
    {
      audioLogger->println(F("ERROR: Unable to set I2S pins\n"));
      return false;
    }
    
    i2s_zero_dma_buffer((i2s_port_t)portNo);
    i2s_start((i2s_port_t)portNo);
  }

  i2sOn = true;
  SetRate(hertz);
  return true;
}

bool AudioOutputI2SNoDACS3::ConsumeSample(int16_t sample[2])
{

  //return if we haven't called ::begin yet
  if (!i2sOn)
    return false;

  int16_t ms[2];

  ms[0] = sample[0];
  ms[1] = sample[1];
  MakeSampleStereo16( ms );

  if (this->mono) {
    // Average the two samples and overwrite
    int32_t ttl = ms[LEFTCHANNEL] + ms[RIGHTCHANNEL];
    ms[LEFTCHANNEL] = ms[RIGHTCHANNEL] = (ttl>>1) & 0xffff;
  }

  uint32_t s32 = ((Amplify(ms[RIGHTCHANNEL])) << 16) | (Amplify(ms[LEFTCHANNEL]) & 0xffff);
  
  size_t i2s_bytes_written;
  i2s_write((i2s_port_t)portNo, (const char*)&s32, sizeof(uint32_t), &i2s_bytes_written, 0);
  return i2s_bytes_written;
}

void AudioOutputI2SNoDACS3::flush()
{
  // makes sure that all stored DMA samples are consumed / played
  int buffersize = 128 * this->dma_buf_count;
  int16_t samples[2] = {0x0, 0x0};
  for (int i = 0; i < buffersize; i++)
  {
    while (!ConsumeSample(samples))
    {
      delay(10);
    }
  }
}

bool AudioOutputI2SNoDACS3::stop()
{
  if (!i2sOn)
    return false;

  i2s_zero_dma_buffer((i2s_port_t)portNo);

  audioLogger->printf("UNINSTALL I2S\n");
  i2s_driver_uninstall((i2s_port_t)portNo); //stop & destroy i2s driver

  i2sOn = false;
  return true;
}

#endif // CONFIG_IDF_TARGET_ESP32S3
