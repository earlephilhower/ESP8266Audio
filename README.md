# ESP8266Audio
Arduino library for parsing and decoding MOD, WAV, and MP3 files and playing them on an I2S DAC or even using a software-simulated delta-sigma DAC with dynamic 32x oversampling

## Disclaimer
All this code is released under the GPL, and all of it is to be used at your own risk.  If you find any bugs, please let me know via the GitHub issue tracker or drop me an email.  The MOD and MP3 routines were taken from StellaPlayer and libMAD respectively.  The software I2S delta-sigma 32x oversampling DAC was my own creation, and sounds quite good if I do say so myself.

## Usage
Create an AudioInputXXX source pointing to your input file, an AudioOutputXXX sink as either an I2SDAC, I2S-sw-DAC, or as a "SerialWAV" which simply writes a mostly-correct WAV file to the Serial port which can be dumped to a file on your development system, and an AudioGeneratorXXX to actually take that input and decode it and send to the output.

After creation, you need to call the AudioGeneratorXXX::loop() routine from inside your own main loop one or more times.  This will automatically read as much of the file as needed and fill up the I2S buffers and immediately return.  Since this is not interrupt driven, if you have large delay()s in your code, you may end up with hiccups in playback.  Either break large delays into very small ones with calls to AudioGenerator::loop(), or reduce the sampling rate to require fewer samples per second.

## Example
See the examples directory for some simple examples, but the following snippet can play an MP3 file over the simulated I2S DAC:
````
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2SNoDAC *out;
void setup()
{
  WiFi.forceSleepBegin();
  Serial.begin(115200);
  delay(1000);
  SPIFFS.begin();
  file = new AudioFileSourceSPIFFS("/jamonit.mp3");
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
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
````

## Software I2S Delta-Sigma DAC (i.e. playing music with a single transistor and speaker)
For the best fidelity, and stereo to boot, spend the money on a real I2S DAC.  Adafruit makes a great mono one with amplifier, and you can find stereo unamplified ones on eBay or elsewhere quite cheaply.  However, thanks to the software delta-sigma DAC with 32x oversampling (up to 128x if the audio rate is low enough) you can still have pretty good sound!

Use the AudioOutputI2S*No*DAC object instead of the AudioOutputI2SDAC in your code, and the following schematic to drive a 2-3W speaker using a single $0.05 NPN 2N3904 transistor:

````
ESP8266-I2SOUT (Rx) --+     2N3904 (NPN)
                      |      +------+
                      |      |      |     +-|
                      |      |E B C |    / S|
                      |      +|-|-|-+    | P|
                      +-------|-+ +------+ E|
                              |          | A|
ESP8266-GND ---------------- GND     +---+ K| 
                                     |   \ R|
USB 5V ------------------------------+    +-|
````
If you don't have a 5V source available on your ESP model, you can use the 5V from your USB serial adapter, or even the 3V from the ESP8266 (but it'll be lower volume).  Don't try and drive the speaker without the transistor, the ESP8266 pins can't give enough current to drive even a headphone well and you may end up damaging your device.

Connections are as a follows:
````
ESP8266-RX(I2S tx) -- 2N3904 Base
ESP8266-GND        -- 2N3904 Emitter
USB-5V             -- Speaker + Terminal
2N3904-Collector   -- Speaker - Terminal
````

Basically the transistor acts as a switch and requires only a drive of 1/beta (~1/1000 for the transistor specified) times the speaker current.  As shown you've got a max power of ~2.4W into the speaker.

## Porting to other microcontrollers
There's no ESP8266-specific code in the AudioGenerator routines, so porting to other controllers should be relatively easy assuming they have the same endianness as the Xtensa core used.  Drop me a line if you're doing this, I may be able to help point you in the right direction.

## Thanks
Thanks to the authors of StellaPlayer and libMAD for releasing their code freely, and to the maintainers and contributors to the ESP8266 Arduino port.

-Earle F. Philhower, III
 earlephilhower@yahoo.com

