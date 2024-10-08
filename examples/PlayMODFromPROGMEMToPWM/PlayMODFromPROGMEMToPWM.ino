#include <Arduino.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMOD.h"
#include "AudioOutputPWM.h"

#if !defined(ARDUINO_ARCH_RP2040)
void setup() {
  Serial.begin(115200);
  Serial.printf("Only for the RP2040/Raspberry Pi Pico\n");
}

void loop() {
}
#else

// 5_steps.mod sample from the mod archive: https://modarchive.org/
#include "5steps.h"

AudioGeneratorMOD *mod;
AudioFileSourcePROGMEM *file;
AudioOutputPWM *out;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  audioLogger = &Serial;
  file = new AudioFileSourcePROGMEM(steps_mod, sizeof(steps_mod));
  out = new AudioOutputPWM();
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
#endif
