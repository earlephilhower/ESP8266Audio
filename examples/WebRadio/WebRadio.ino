/*
  WebRadio Example
  Very simple HTML app to control web streaming
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioOutputI2SDAC.h"
#include "ESP8266WebServer.h"

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.

// Enter your WiFi setup here:
const char *SSID = "....";
const char *PASSWORD = "....";

ESP8266WebServer server(80);

AudioGenerator *decoder;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2SDAC *out;

int volume = 100;
char title[64];
char url[64];
char status[64];

// C++11 multiline string constants are neato...
const char INDEX[] PROGMEM = R"KEWL(<html>
<head>
<title>ESP8266 Web Radio</title>
</head>
<body>
ESP8266 Web Radio!
<hr>
Currently Playing: <span id="titlespan">%s</span><br>
<script type="text/javascript">
  function updateTitle() {
    var x = new XMLHttpRequest();
    x.open("GET", "title");
    x.onload = function() { document.getElementById("titlespan").innerHTML=x.responseText; setTimeout(updateTitle, 1000); }
    x.send();
  }
  setTimeout(updateTitle, 1000);
</script>
Volume: <input type="range" name="vol" min="1" max="150" steps="10" value="%d" onchange="showValue(this.value)"/> <span id="volspan">%d</span>%%
<script type="text/javascript">
  function showValue(n) {
    document.getElementById("volspan").innerHTML=n;
    var x = new XMLHttpRequest();
    x.open("GET", "setvol?vol="+n);
    x.send();
  }
</script>
<hr>
Status: %s
<hr>
<form action="changeurl" method="POST">
Current URL: %s<br>
Change URL: <input type="text" name="url">
<select name="type"><option value="mp3">MP3</option><option value="aac">AAC</option></select>
<input type="submit" value="Change"></form>
</body>
</html>)KEWL";

// Static spot to store the web sprintf's

void IndexHTML()
{
  char *webbuff = (char*)malloc(2048);
  snprintf_P(webbuff, 2048, INDEX, title, volume, volume, status, url);
  webbuff[2047] = 0;
  server.send(200, "text/html", webbuff );
  free(webbuff);
}

void Send404()
{
  server.send(404, "text/plain", "404: Not Found");
}

void GetTitle()
{
  server.send(200, "text/plain", title);
}

void SetVol()
{
  if (server.hasArg("vol")) {
    int vol = 100;
    int ret = sscanf(server.arg("vol").c_str(), "%d", &vol);
    if (ret==1) {
      if (vol < 0) vol = 0;
      if (vol > 150) vol = 150;
      if (out) out->SetGain(((float)vol)/100.0);
      volume = vol;
      Serial.printf("Set volume: %d\n", volume);
      server.sendHeader("Location", "/", true);
      server.send(301, "text/plain", "");
    }
  }
}

void ChangeURL()
{
  if (server.hasArg("url") && server.hasArg("type")) {
    // Stop and free existing ones
    if (decoder) {
      decoder->stop();
      delete decoder;
    }
    if (buff) {
      buff->close();
      delete buff;
    }
    if (out) {
      out->stop();
      delete out;
    }
    if (file) {
      file->close();
      delete file;
    }

    strncpy(url, server.arg("url").c_str(), sizeof(url)-1);
    url[sizeof(url)-1] = 0;
    file = new AudioFileSourceICYStream(server.arg("url").c_str());
    file->RegisterMetadataCB(MDCallback, (void*)"ICY");
    buff = new AudioFileSourceBuffer(file, 2048);
    buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
    out = new AudioOutputI2SDAC();
    if (!strcmp(server.arg("type").c_str(), "aac")) {
      decoder = new AudioGeneratorAAC();
    } else {
      decoder = new AudioGeneratorMP3();
    }
    decoder->RegisterStatusCB(StatusCallback, (void*)"mp3");
    decoder->begin(buff, out);
    out->SetGain(((float)volume)/100.0);
    server.sendHeader("Location", "/", true);
    server.send(301, "text/plain", "");
  } else {
    Send404(); // Need to specify those params or you're in error!
  }
}
void MDCallback(void *cbData, const char *type, bool isUnicode, Stream *stream)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  if (strstr(type, "Title")) { 
    strncpy(title, stream->readString().c_str(), sizeof(title));
    title[sizeof(title)-1] = 0;
    Serial.printf("Set title: %s\n", title);
  } else {
    Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, type, stream->readString().c_str());
  }
  
  Serial.flush();
}
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, string);
  strncpy(status, string, sizeof(status)-1);
  status[sizeof(status)-1] = 0;
  Serial.flush();
}

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
  Serial.println("Connected\n");
  
  Serial.print("Go to http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ to control the web radio.");

  server.on("/", HTTP_GET, IndexHTML);
  server.on("/setvol", HTTP_GET, SetVol);
  server.on("/title", HTTP_GET, GetTitle);
  server.on("/changeurl", HTTP_POST, ChangeURL);
  server.onNotFound(Send404);
  server.begin();

  strcpy(url, "none");
  strcpy(status, "OK");
  strcpy(title, "Idle");

  file = NULL;
  buff = NULL;
  out = NULL;
  decoder = NULL;
}

void loop()
{
  static int lastms = 0;
  server.handleClient();
  if (!decoder) return;
  
  if (decoder->isRunning()) {
    strcpy(status, "OK"); // By default we're OK unless the decoder says otherwise
    if (millis()-lastms > 1000) {
      lastms = millis();
      Serial.printf("Running for %d seconds...\n", lastms/1000);
      Serial.flush();
     }
    if (!decoder->loop()) {
      Serial.println("Stopping decoder");
      decoder->stop();
      strcpy(title, "Stopped");
    }
  } else {
   // Nothing here
  }
}

