/*
  AudioInputI2S
  I2S audio source

*/

#ifndef _AUDIOINPUTI2S_H
#define _AUDIOINPUTI2S_H

#include <Arduino.h>
#include "AudioStatus.h"
#include "AudioOutput.h"

class AudioInputI2S
{
  public:
    AudioInputI2S(int port=0, int dma_buf_count = 8, int use_apll=APLL_DISABLE);
    virtual ~AudioInputI2S();
    bool SetPinout(int bclkPin, int wclkPin, int dinPin);
    virtual bool begin(AudioOutput *output);
    virtual bool loop();
    virtual bool stop();
    virtual bool isRunning();
    virtual bool SetRate(int hz);
    virtual bool SetBitsPerSample(int bits);
    virtual bool SetGain(float f);
    uint32_t GetSample(void);
    virtual uint32_t read(void* data, size_t len_bytes);

    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
  private:
  protected:
    uint8_t portNo;
    bool i2sOn;
    uint16_t hertz;
    uint8_t bps;
    uint8_t channels;
    AudioOutput *output;
    int16_t buffLen;
    uint8_t *buff;
    int16_t validSamples;
    int16_t curSample;
    int gain_shift;
};

#endif // _AUDIOINPUTI2S_H
