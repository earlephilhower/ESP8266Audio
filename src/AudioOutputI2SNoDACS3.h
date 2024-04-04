/*
  AudioOutputI2S
  Base class for an I2S output port
  
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

#pragma once

#include "AudioOutput.h"
#include "driver/i2s.h"

#if CONFIG_IDF_TARGET_ESP32S3
class AudioOutputI2SNoDACS3 : public AudioOutput
{
  public:
    /// @brief No-DAC Audio Output for ESP32-S3
    /// @param doutPin Pin to modulate audio on (PDM data)
    /// @param dummyPin Dummy pin to use for WS/BCLK (PDM clock)
    /// @param port 
    /// @param dma_buf_count 
    AudioOutputI2SNoDACS3(int doutPin, int dummyPin = GPIO_NUM_3, int port=0, int dma_buf_count = 8);

    virtual ~AudioOutputI2SNoDACS3() override;
    virtual bool SetRate(int hz) override;
    virtual bool SetBitsPerSample(int bits) override;
    virtual bool SetChannels(int channels) override;
    virtual bool begin() override;
    virtual bool ConsumeSample(int16_t sample[2]) override;
    virtual void flush() override;
    virtual bool stop() override;
    
    bool SetOutputModeMono(bool mono);  // Force mono output no matter the input

  protected:
    virtual int AdjustI2SRate(int hz) { return (int)((float)hz / 1.0f); }
    uint8_t portNo;
    bool mono;
    bool i2sOn;
    int dma_buf_count;
    
    gpio_num_t doutPin;
    gpio_num_t dummyPin;

#if defined(ARDUINO_ARCH_RP2040)
    I2S i2s;
#endif
};
#endif