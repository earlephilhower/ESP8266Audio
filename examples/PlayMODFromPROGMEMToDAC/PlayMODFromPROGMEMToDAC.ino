#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMOD.h"
#include "AudioOutputI2SDAC.h"

// enigma.mod sample from the mod archive: https://modarchive.org/index.php?request=view_by_moduleid&query=42146
#include "enigma.h"

AudioGeneratorMOD *mod;
AudioFileSourcePROGMEM *file;
AudioOutputI2SDAC *out;

void setup()
{
  WiFi.forceSleepBegin();
  Serial.begin(115200);
  delay(1000);
  file = new AudioFileSourcePROGMEM( enigma_mod, sizeof(enigma_mod) );
  out = new AudioOutputI2SDAC();
  mod = new AudioGeneratorMOD();
  mod->SetBufferSize(3*1024);
  mod->SetSampleRate(44100);
  mod->SetStereoSeparation(32);
  mod->begin(file, out);
}

void loop()
{
  if (mod->isRunning()) {
    if (!mod->loop()) mod->stop();
  } else {
    Serial.printf("MOD done\n");
    delay(1000);
  }
}

