/*
  AudioInputI2S
  I2S audio source

*/

#include <Arduino.h>
#ifdef ESP32
  #include "driver/i2s.h"
#else
  #include <i2s.h>
#endif
#include "AudioInputI2S.h"

AudioInputI2S::AudioInputI2S(int port, int dma_buf_count, int use_apll)
{
  this->portNo = port;
  this->running = false;
  this->hertz = 44100;

  buffLen = dma_buf_count*64;
  buff = (uint8_t*)malloc(buffLen);

#ifdef ESP32
  if (!running) {
    if (use_apll == APLL_AUTO) {
      // don't use audio pll on buggy rev0 chips
      use_apll = APLL_DISABLE;
      esp_chip_info_t out_info;
      esp_chip_info(&out_info);
      if(out_info.revision > 0) {
        use_apll = APLL_ENABLE;
      }
    }

    i2s_config_t i2s_config_adc = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = hertz,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
      .dma_buf_count = dma_buf_count,
      .dma_buf_len = 64,
      .use_apll = use_apll // Use audio PLL
    };
    Serial.printf("+%d %p\n", portNo, &i2s_config_adc);
    if (i2s_driver_install((i2s_port_t)portNo, &i2s_config_adc, 0, NULL) != ESP_OK) {
      Serial.println("ERROR: Unable to install I2S drives\n");
    }
    //SetPinout(21, 25, 19); // be careful not to overwrite the other i2s channel

    i2s_zero_dma_buffer((i2s_port_t)portNo);
  } 
#else
  (void) use_apll;
  if (!running) {
    i2s_begin();
  }
#endif
  running = true;
  bps = 32;
  channels = 1;
  gain_shift = 0;
}

AudioInputI2S::~AudioInputI2S()
{
#ifdef ESP32
  if (running) {
    Serial.printf("UNINSTALL I2S\n");
    i2s_driver_uninstall((i2s_port_t)portNo); //stop & destroy i2s driver
  }
#else
  if (running) i2s_end();
#endif
running = false;
free(buff);
}

uint32_t AudioInputI2S::GetSample()
{
  if (!running) return 0;

  uint32_t sampleIn=0;
#ifdef ESP32
  i2s_pop_sample((i2s_port_t)portNo, (char*)&sampleIn, portMAX_DELAY);
  return sampleIn>>(14+gain_shift);
#else
  i2s_read_sample(&lastSample[LEFTCHANNEL], &lastSample[RIGHTCHANNEL], true);
  return (lastSample[RIGHTCHANNEL] << 16) | (lastSample[LEFTCHANNEL] & 0xffff);
#endif
}

uint32_t AudioInputI2S::read(void* data, size_t len_bytes)
{
  if (!running) return 0;
  size_t bytes_read = 0;
  //bytes_read = i2s_read_bytes((i2s_port_t)portNo, (char*)data, (size_t)len, portMAX_DELAY);
#ifdef ESP32
  esp_err_t err = i2s_read((i2s_port_t)portNo, data, len_bytes, &bytes_read, 0);
#else
  uint16_t* data16 = reinterpret_cast<uint16_t*>(data);
  for (int i = 0; (i < len_bytes/(2*sizeof(int16_t))) && (i2s_rx_available()) > 0; i++) {
    i2s_read_sample(&lastSample[LEFTCHANNEL], &lastSample[RIGHTCHANNEL], false);
    data16[i+RIGHTCHANNEL] = lastSample[RIGHTCHANNEL];
    data16[i+LEFTCHANNEL] = lastSample[LEFTCHANNEL];
  }
#endif
  return bytes_read;
}

bool AudioInputI2S::SetPinout(int bclk, int wclk, int din)
{
#ifdef ESP32
  i2s_pin_config_t pin_config = {
    .bck_io_num = bclk,
    .ws_io_num = wclk,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = din
  };
  i2s_set_pin((i2s_port_t)portNo, &pin_config);
  return true;
#else
  (void) bclk;
  (void) wclk;
  (void) din;
  return false;
#endif
}

bool AudioInputI2S::SetRate(int hz)
{
  // TODO - have a list of allowable rates from constructor, check them
  this->hertz = hz;
#ifdef ESP32
  i2s_set_sample_rates((i2s_port_t)portNo, hz); 
#else
  i2s_set_rate(hz);
#endif
  return true;
}

bool AudioInputI2S::SetBitsPerSample(int bits)
{
  if ( (bits != 32) && (bits != 16) && (bits != 8) ) return false;
  this->bps = bits;
  return true;
}

bool AudioInputI2S::SetGain(float f)
{
  if (f < 0.33) this->gain_shift = 2;
  else if (f < 0.66) this->gain_shift = 1;
  else this->gain_shift = 0;
  return true;
}

bool AudioInputI2S::stop() {
  running = false;
  output->stop();
#ifdef ESP32
  esp_err_t err = i2s_stop((i2s_port_t)portNo);
  return (err == ESP_OK);
#else
  i2s_rxtx_begin(false, false);
  return true;
#endif
}

bool AudioInputI2S::isRunning() {
  return running;
}

bool AudioInputI2S::loop() {
  if (!running) return false;

  uint32_t* buff32 = reinterpret_cast<uint32_t*>(buff);

  while (validSamples) {
    int32_t sample = buff32[curSample]>>(14+gain_shift);
    lastSample[0] = sample;
    lastSample[1] = sample;
    if (!output->ConsumeSample(lastSample)) {
      output->loop();
      yield();
      return true;
    }
    validSamples--;
    curSample++;
  }

  validSamples = read(buff, buffLen) / sizeof(uint32_t);
  curSample = 0;

  output->loop();
}

bool AudioInputI2S::begin(AudioFileSource *source, AudioOutput *output) {
  if (!output) return false;
  if (!running) {
#ifdef ESP32
    esp_err_t err = i2s_start((i2s_port_t)portNo);
    if (err != ESP_OK) return false;
#else
    i2s_rxtx_begin(true, false);
#endif
    running = true;
  }
  this->output = output;
  output->begin();
  output->SetBitsPerSample(bps);
  output->SetRate(hertz);
  return true;
}
