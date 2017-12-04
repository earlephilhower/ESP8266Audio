#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

// To run, set your ESP8266 build to 160MHz, and include a SPIFFS of 512KB or greater.
// Use the "Tools->ESP8266 Sketch Data Upload" menu to write the MP3 to SPIFFS
// Then upload the sketch normally.  

// pno_cs from https://ccrma.stanford.edu/~jos/pasp/Sound_Examples.html

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2SNoDAC *out;
AudioFileSourceID3 *id3;

void ID3Callback(const char *type, bool unicode, int len, AudioFileSource *src)
{
  Serial.printf("ID3 callback for: %s = [%d bytes] '", type, len);
  if (unicode && len > 2) {
    char ign[2];
    src->read(&ign, 2);
    len -= 2;
  }
  while (len) {
    char a, b;
    src->read(&a, 1);
    len--;
    if (unicode && len) {
      src->read(&b, 1);
      len--;
    }
    Serial.printf("%c", a);
  }
  Serial.printf("'\n");
  Serial.flush();
}


void setup()
{
  WiFi.forceSleepBegin();
  Serial.begin(115200);
  delay(1000);
  SPIFFS.begin();
  Serial.printf("Sample MP3 playback begins...\n");
  file = new AudioFileSourceSPIFFS("/pno-cs.mp3");
  id3 = new AudioFileSourceID3(file);
  id3->setCallback(ID3Callback);
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}

void loop()
{
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
  }
}
