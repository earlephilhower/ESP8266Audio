/*
  AudioOutputI2S
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
#ifdef ESP32
  #include "driver/i2s.h"
#elif defined(ARDUINO_ARCH_RP2040) || ARDUINO_ESP8266_MAJOR >= 3
  #include <I2S.h>
#elif ARDUINO_ESP8266_MAJOR < 3
  #include <i2s.h>
#endif
#include "AudioOutputI2S.h"

#if defined(ESP32) || defined(ESP8266)
AudioOutputI2S::AudioOutputI2S(int port, int output_mode, int dma_buf_count, int use_apll)
{
  this->portNo = port;
  this->i2sOn = false;
  this->dma_buf_count = dma_buf_count;
  if (output_mode != EXTERNAL_I2S && output_mode != INTERNAL_DAC && output_mode != INTERNAL_PDM) {
    output_mode = EXTERNAL_I2S;
  }
  this->output_mode = output_mode;
  this->use_apll = use_apll;

  //set defaults
  mono = false;
  lsb_justified = false;
  bps = 16;
  channels = 2;
  hertz = 44100;
  bclkPin = 26;
  wclkPin = 25;
  doutPin = 22;
  mclkPin = 0;
  SetGain(1.0);
}

#elif defined(ARDUINO_ARCH_RP2040)
AudioOutputI2S::AudioOutputI2S(long sampleRate, pin_size_t sck, pin_size_t data) {
    i2sOn = false;
    mono = false;
    bps = 16;
    channels = 2;
    hertz = sampleRate;
    bclkPin = sck;
    doutPin = data;
    mclkPin = 0;
    use_mclk = false;
    swap_clocks = false;
    SetGain(1.0);
}
#endif

AudioOutputI2S::~AudioOutputI2S()
{
  stop();
}

bool AudioOutputI2S::SetPinout()
{
  #ifdef ESP32
    if (output_mode == INTERNAL_DAC || output_mode == INTERNAL_PDM)
      return false; // Not allowed

    i2s_pin_config_t pins = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .mck_io_num = mclkPin,
#endif
        .bck_io_num = bclkPin,
        .ws_io_num = wclkPin,
        .data_out_num = doutPin,
        .data_in_num = I2S_PIN_NO_CHANGE};
    i2s_set_pin((i2s_port_t)portNo, &pins);
    return true;
  #else
    (void)bclkPin;
    (void)wclkPin;
    (void)doutPin;
    (void)mclkPin;
    (void)use_mclk;
    return false;
  #endif
}

bool AudioOutputI2S::SetPinout(int bclk, int wclk, int dout)
{
  bclkPin = bclk;
  wclkPin = wclk;
  doutPin = dout;
  if (i2sOn)
    return SetPinout();

  return true;
}

bool AudioOutputI2S::SetPinout(int bclk, int wclk, int dout, int mclk)
{
  bclkPin = bclk;
  wclkPin = wclk;
  doutPin = dout;
  #if defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
    mclkPin = mclk;
    if (i2sOn)
      return SetPinout();
  #else
    (void)mclk;
  #endif
  return true;
}

bool AudioOutputI2S::SetRate(int hz)
{
  // TODO - have a list of allowable rates from constructor, check them
  this->hertz = hz;
  if (i2sOn)
  {
  #ifdef ESP32
      i2s_set_sample_rates((i2s_port_t)portNo, AdjustI2SRate(hz));
  #elif defined(ESP8266)
      i2s_set_rate(AdjustI2SRate(hz));
  #elif defined(ARDUINO_ARCH_RP2040)
      i2s.setFrequency(hz);
  #endif
  }
  return true;
}

bool AudioOutputI2S::SetBitsPerSample(int bits)
{
  if ( (bits != 16) && (bits != 8) ) return false;
  this->bps = bits;
  return true;
}

bool AudioOutputI2S::SetChannels(int channels)
{
  if ( (channels < 1) || (channels > 2) ) return false;
  this->channels = channels;
  return true;
}

bool AudioOutputI2S::SetOutputModeMono(bool mono)
{
  this->mono = mono;
  return true;
}

bool AudioOutputI2S::SetLsbJustified(bool lsbJustified)
{
  this->lsb_justified = lsbJustified;
  return true;
}

bool AudioOutputI2S::SwapClocks(bool swap_clocks)
{
  if (i2sOn) {
    return false; // Not allowed
  }
  this->swap_clocks = swap_clocks;
  return true;
}

bool AudioOutputI2S::SetMclk(bool enabled){
  (void)enabled;
  #ifdef ESP32
    if (output_mode == INTERNAL_DAC || output_mode == INTERNAL_PDM)
      return false; // Not allowed

    use_mclk = enabled;
  #elif defined(ARDUINO_ARCH_RP2040)
    use_mclk = enabled;
  #endif
  return true;
}

bool AudioOutputI2S::begin(bool txDAC)
{
  #ifdef ESP32
    if (!i2sOn)
    {
      if (use_apll == APLL_AUTO)
      {
        // don't use audio pll on buggy rev0 chips
        use_apll = APLL_DISABLE;
        //esp_chip_info_t out_info;
        //esp_chip_info(&out_info);
        //if (out_info.revision > 0)
        {
          use_apll = APLL_ENABLE;
        }
      }

      i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
      if (output_mode == INTERNAL_DAC)
      {
#if CONFIG_IDF_TARGET_ESP32
        mode = (i2s_mode_t)(mode | I2S_MODE_DAC_BUILT_IN);
#else
        return false;      
#endif
      }
      else if (output_mode == INTERNAL_PDM)
      {
#if CONFIG_IDF_TARGET_ESP32
        mode = (i2s_mode_t)(mode | I2S_MODE_PDM);
#else
        return false;      
#endif
      }

      i2s_comm_format_t comm_fmt;
      if (output_mode == INTERNAL_DAC)
      {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        comm_fmt = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_MSB;
#else
        comm_fmt = (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
#endif
      }
      else if (lsb_justified)
      {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        comm_fmt = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_MSB;
#else
        comm_fmt = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB);
#endif
      }
      else
      {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        comm_fmt = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S);
#else
        comm_fmt = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
#endif
      }

      i2s_config_t i2s_config_dac = {
          .mode = mode,
          .sample_rate = 44100,
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = comm_fmt,
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
          .dma_buf_count = dma_buf_count,
          .dma_buf_len = 128,
          .use_apll = use_apll, // Use audio PLL
          .tx_desc_auto_clear = true, // Silence on underflow
          .fixed_mclk = use_mclk, // Unused
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
          .mclk_multiple = I2S_MCLK_MULTIPLE_256, // Unused
          .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT // Use bits per sample
#endif
      };
      audioLogger->printf("+%d %p\n", portNo, &i2s_config_dac);
      if (i2s_driver_install((i2s_port_t)portNo, &i2s_config_dac, 0, NULL) != ESP_OK)
      {
        audioLogger->println("ERROR: Unable to install I2S drives\n");
      }
      if (output_mode == INTERNAL_DAC || output_mode == INTERNAL_PDM)
      {
#if CONFIG_IDF_TARGET_ESP32
        i2s_set_pin((i2s_port_t)portNo, NULL);
        i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
#else
        return false;
#endif
      }
      else
      {
        SetPinout();
      }
      i2s_zero_dma_buffer((i2s_port_t)portNo);
    }
  #elif defined(ESP8266)
    (void)dma_buf_count;
    (void)use_apll;
    if (!i2sOn)
    {
      orig_bck = READ_PERI_REG(PERIPHS_IO_MUX_MTDO_U);
      orig_ws = READ_PERI_REG(PERIPHS_IO_MUX_GPIO2_U);
    #ifdef I2S_HAS_BEGIN_RXTX_DRIVE_CLOCKS
      if (!i2s_rxtxdrive_begin(false, true, false, txDAC)) {
        return false;
      }
    #else
      if (!i2s_rxtx_begin(false, true)) {
        return false;
      }
      if (!txDAC) {
        audioLogger->printf_P(PSTR("I2SNoDAC: esp8266 arduino core should be upgraded to avoid conflicts with SPI\n"));
      }
    #endif
    }
  #elif defined(ARDUINO_ARCH_RP2040)
    (void)txDAC;
    if (!i2sOn) {
      i2s.setSysClk(hertz);
      i2s.setBCLK(bclkPin);
      i2s.setDATA(doutPin);
      i2s.setMCLK(mclkPin);
      i2s.setMCLKmult(256);
      if (swap_clocks) {
        i2s.swapClocks();
      }
      i2s.setBitsPerSample(bps);
      i2s.begin(hertz);
    }
  #endif
  i2sOn = true;
  SetRate(hertz); // Default
  return true;
}

bool AudioOutputI2S::ConsumeSample(int16_t sample[2])
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
  #ifdef ESP32
    uint32_t s32;
    if (output_mode == INTERNAL_DAC)
    {
      int16_t l = Amplify(ms[LEFTCHANNEL]) + 0x8000;
      int16_t r = Amplify(ms[RIGHTCHANNEL]) + 0x8000;
      s32 = ((r & 0xffff) << 16) | (l & 0xffff);
    }
    else
    {
      s32 = ((Amplify(ms[RIGHTCHANNEL])) << 16) | (Amplify(ms[LEFTCHANNEL]) & 0xffff);
    }
//"i2s_write_bytes" has been removed in the ESP32 Arduino 2.0.0,  use "i2s_write" instead.
//    return i2s_write_bytes((i2s_port_t)portNo, (const char *)&s32, sizeof(uint32_t), 0);

    size_t i2s_bytes_written;
    i2s_write((i2s_port_t)portNo, (const char*)&s32, sizeof(uint32_t), &i2s_bytes_written, 0);
    return i2s_bytes_written;
  #elif defined(ESP8266)
    uint32_t s32 = ((Amplify(ms[RIGHTCHANNEL])) << 16) | (Amplify(ms[LEFTCHANNEL]) & 0xffff);
    return i2s_write_sample_nb(s32); // If we can't store it, return false.  OTW true
  #elif defined(ARDUINO_ARCH_RP2040)
    uint32_t s32 = ((Amplify(ms[RIGHTCHANNEL])) << 16) | (Amplify(ms[LEFTCHANNEL]) & 0xffff);
    return !!i2s.write((int32_t)s32, false);
  #endif
}

void AudioOutputI2S::flush()
{
  #ifdef ESP32
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
  #elif defined(ARDUINO_ARCH_RP2040)
    i2s.flush();
  #endif
}

bool AudioOutputI2S::stop()
{
  if (!i2sOn)
    return false;

  #ifdef ESP32
    i2s_zero_dma_buffer((i2s_port_t)portNo);
    audioLogger->printf("UNINSTALL I2S\n");
    i2s_driver_uninstall((i2s_port_t)portNo); //stop & destroy i2s driver
  #elif defined(ESP8266)
    i2s_end();
  #elif defined(ARDUINO_ARCH_RP2040)
    i2s.end();
  #endif
  i2sOn = false;
  return true;
}
