#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceSPIRAMBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.

// Enter your WiFi setup here:
const char *SSID = ".....";
const char *PASSWORD = ".....";

// Randomly picked URL
const char *URL="http://streaming.shoutcast.com/80sPlanet?lang=en-US";

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceSPIRAMBuffer *buff;
AudioOutputI2SNoDAC *out;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi");

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  WiFi.hostname("melody");
  
  byte zero[] = {0,0,0,0};
  WiFi.config(zero, zero, zero, zero);

  WiFi.begin(SSID, PASSWORD);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }

  file = new AudioFileSourceHTTPStream(URL);
  // Initialize 23LC1024 SPI RAM buffer with chip select ion GPIO4 and ram size of 128KByte
  buff = new AudioFileSourceSPIRAMBuffer(file, 4, 131072);
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(buff, out);
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

