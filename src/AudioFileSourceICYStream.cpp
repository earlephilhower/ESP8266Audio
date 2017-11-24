/*
  AudioFileSourceICYStream
  Streaming Shoutcast ICY source
  
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

#include "AudioFileSourceICYStream.h"

AudioFileSourceICYStream::AudioFileSourceICYStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL = NULL;
}

AudioFileSourceICYStream::AudioFileSourceICYStream(const char *url)
{
  saveURL = NULL;
  reconnectTries = 0;
  open(url);
}

bool AudioFileSourceICYStream::open(const char *url)
{
  static const char *hdr[] = { "icy-metaint" };
  pos = 0;
  http.begin(url);
  http.addHeader("Icy-MetaData", "1");
  http.collectHeaders( hdr, 1 );
  http.setReuse(true);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  if (http.hasHeader(hdr[0])) {
    String ret = http.header(hdr[0]);
    icyMetaInt = ret.toInt();
    Serial.printf("ICY metaint = %d\n", icyMetaInt);
  } else {
    icyMetaInt = 0;
  }
  icyByteCount = 0;
  size = http.getSize();
  Serial.printf("Stream size: %d\n", size);
  free(saveURL);
  saveURL = strdup(url);
  return true;
}

AudioFileSourceICYStream::~AudioFileSourceICYStream()
{
  http.end();
}

uint32_t AudioFileSourceICYStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    Serial.println("Stream disconnected\n");
    Serial.flush();
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      Serial.printf("Attempting to reconnect, try %d\n", i);
      Serial.flush();
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        Serial.println("Reconnected to stream");
        break;
      }
    }
    if (!http.connected()) {
      Serial.printf("Unable to reconnect\n");
      Serial.flush();
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    Serial.printf("No stream data available\n");
    Serial.flush();
    http.end();
    goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = 0;
  // If the read would hit an ICY block, split it up...
  if (((int)(icyByteCount + len) > (int)icyMetaInt) && (icyMetaInt > 0)) {
    int beforeIcy = icyMetaInt - icyByteCount;
    int ret = stream->readBytes(reinterpret_cast<uint8_t*>(data), beforeIcy);
    read += ret;
    pos += ret;
    len -= ret;
    data = (void *)(reinterpret_cast<char*>(data) + ret);
    icyByteCount += ret;
    if (ret != beforeIcy) return read; // Partial read

    uint8_t mdSize;
    int mdret = stream->readBytes(&mdSize, 1);
    if ((mdret == 1) && (mdSize > 0)) {
      char mdBuff[17];
      mdBuff[16] = 0;
      Serial.print("ICY MD: ");
      for (int i=0; i<mdSize; i++) {
        mdret = stream->readBytes(reinterpret_cast<uint8_t*>(mdBuff), 16);
        if (mdret == 16) {
          Serial.printf("%s", mdBuff);
        }
      }
      Serial.println("\n---end---");
    }
    icyByteCount = 0;
  }

  int ret = stream->readBytes(reinterpret_cast<uint8_t*>(data), len);
  read += ret;
  pos += ret;
  icyByteCount += ret;
  return read;
}
