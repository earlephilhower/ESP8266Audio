#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorOpus.h"
#include "AudioOutputI2S.h"

// The includes OPUS file is from Kevin MacLeod (incompetech.com), Licensed under Creative Commons: By Attribution 3.0, http://creativecommons.org/licenses/by/3.0/

AudioGeneratorOpus *opus;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;

void setup()
{
  WiFi.mode(WIFI_OFF); 
  Serial.begin(115200);
  delay(1000);
  LittleFS.begin();
  Serial.printf("Sample Opus playback begins...\n");

  audioLogger = &Serial;
  file = new AudioFileSourceSPIFFS("/gs-16b-2c-44100hz.opus");
  out = new AudioOutputI2S();
  opus = new AudioGeneratorOpus();
  opus->begin(file, out);
}

void loop()
{
  if (opus->isRunning()) {
    if (!opus->loop()) opus->stop();
  } else {
    Serial.printf("Opus done\n");
    delay(1000);
  }
}
