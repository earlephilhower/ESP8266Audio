#include <Arduino.h>

#include<SPI.h>
#ifdef ESP32
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif

#include "AudioInputI2S.h"
#include "AudioOutputI2S.h"
#include "AudioOutputBuffer.h"

AudioInputI2S *in;
AudioOutputI2S *out;
AudioOutputBuffer *outbuf;

void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  out = new AudioOutputI2S(0);
  in = new AudioInputI2S(1);
  out->SetPinout(26, 25, 22);
  in->SetPinout(23, 18, 5);
  //out->SetGain(0.02);
  //in->SetGain(0.1);

  outbuf = new AudioOutputBuffer(0.7*44100, out);
  in->begin(outbuf);
  Serial.printf("starting\n");
}

void loop()
{
  if (in->isRunning()) {
    if (!in->loop()) in->stop();
  }
  delay(5);
}
