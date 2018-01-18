#include <Arduino.h>
#ifdef ESP32
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif


#include <AudioOutputNull.h>
#include <AudioOutputI2SDAC.h>
#include <AudioGeneratorMIDI.h>
#include <AudioFileSourceFastROMFS.h>

AudioFileSourceFastROMFS *sf2;
AudioFileSourceFastROMFS *mid;
AudioOutputI2SDAC *dac;
AudioGeneratorMIDI *midi;
FastROMFilesystem *fs;

void setup()
{
  const char *soundfont = "1mgm.sf2";
  const char *midifile = "furelise.mid";

  WiFi.mode(WIFI_OFF); 

  Serial.begin(115200);
  Serial.println("Starting up...\n");

  fs = new FastROMFilesystem();
  fs->mount();

  sf2 = new AudioFileSourceFastROMFS(fs, soundfont);
  mid = new AudioFileSourceFastROMFS(fs, midifile);
  
  dac = new AudioOutputI2SDAC();
  midi = new AudioGeneratorMIDI();
  midi->SetSoundfont(sf2);
  midi->SetSampleRate(22050);
  Serial.printf("BEGIN...\n");
  midi->begin(mid, dac);
}

void loop()
{
  if (midi->isRunning()) {
    if (!midi->loop()) {
      uint32_t e = millis();
      midi->stop();
    }
  } else {
    Serial.printf("MIDI done\n");
    delay(1000);
  }
}


