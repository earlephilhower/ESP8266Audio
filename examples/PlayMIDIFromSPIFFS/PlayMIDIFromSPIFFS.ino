#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <AudioOutputNull.h>
#include <AudioOutputI2SNoDAC.h>
#include <AudioGeneratorMIDI.h>
#include <AudioFileSourceSPIFFS.h>

#include <FS.h>

/**
 * Midi to  I2S NoDAC
 * Force a 160Mhz clock speed 
 */
AudioFileSourceSPIFFS *sf2;
AudioFileSourceSPIFFS *mid;
AudioOutputI2SNoDAC *dac;
AudioGeneratorMIDI *midi;

void startSound(){
  
  
  sf2 = new AudioFileSourceSPIFFS("/1mgm.sf2");
  mid = new AudioFileSourceSPIFFS("/furelise.mid");
  
  dac = new AudioOutputI2SNoDAC();
  midi = new AudioGeneratorMIDI();
  midi->SetSoundfont(sf2);
  midi->SetSampleRate(22050);
  Serial.println("BEGIN MIDI...");
  midi->begin(mid, dac);
  Serial.println("MIDI OK");
}

void setup()
{
  WiFi.forceSleepBegin();
  Serial.begin(115200);
  Serial.println("BOOT: Starting up...");
  SPIFFS.begin();
  Serial.println("Files:");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.println(dir.fileName());
    //File f = dir.openFile("r");
    //Serial.println(f.size());
  }
  Serial.println("Ready");
  startSound();
}

void loop()
{
  if (midi->isRunning()) {
    if (!midi->loop()) {
      uint32_t e = millis();
      midi->stop();
    }
  } else {
    Serial.printf("END MIDI\n");
    delay(1000);
    startSound();
  }
}


