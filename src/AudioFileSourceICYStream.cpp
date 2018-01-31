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
  saveURL[0] = 0;
}

AudioFileSourceICYStream::AudioFileSourceICYStream(const char *url)
{
  saveURL[0] = 0;
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
    cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
    return false;
  }
  if (http.hasHeader(hdr[0])) {
    String ret = http.header(hdr[0]);
    icyMetaInt = ret.toInt();
  } else {
    icyMetaInt = 0;
  }
  icyByteCount = 0;
  size = http.getSize();
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL)-1] = 0;
  return true;
}

AudioFileSourceICYStream::~AudioFileSourceICYStream()
{
  http.end();
}

class ICYMDReader {
  public:
    ICYMDReader(WiFiClient *str, uint16_t bytes) {
      stream = str;
      avail = bytes;
      ptr = sizeof(buff)+1; // Cause read next time
      saved = -1;
    }
    ~ICYMDReader() {
      // Get rid of any remaining bytes in the MD block
      char xxx[16];
      if (saved>=0) avail--; // Throw away any unread bytes
      while (avail > 16) {
        stream->read(reinterpret_cast<uint8_t*>(xxx), 16);
        avail -= 16;
      }
      stream->read(reinterpret_cast<uint8_t*>(xxx), avail);
    }
    int read(uint8_t *dest, int len) {
      if (!len) return 0;
      int ret = 0;
      if (saved >= 0) { *(dest++) = (uint8_t)saved; saved = -1; len--; avail--; ret++; }
      while ((len>0) && (avail>0)) {
        // Always copy from bounce buffer first
        while ((ptr < sizeof(buff)) && (len>0) && (avail>0)) {
          *(dest++) = buff[ptr++];
          avail--;
          ret++;
          len--;
        }
        // refill the bounce buffer
        if ((avail>0) && (len>0)) {
          ptr = 0;
          int toRead = (sizeof(buff)>avail)? avail : sizeof(buff);
          int read = stream->read(buff, toRead);
          if (read != toRead) return 0; // Error, short read!
        }
      }
      return ret;
    }
    bool eof() { return (avail==0); }
    bool unread(uint8_t c) { if (saved>=0) return false; saved = c; avail++; return true; }

  private:
    WiFiClient *stream;
    uint16_t avail;
    uint8_t ptr;
    uint8_t buff[16];
    int saved;
};


uint32_t AudioFileSourceICYStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[32];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
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
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = 0;
  // If the read would hit an ICY block, split it up...
  if (((int)(icyByteCount + len) > (int)icyMetaInt) && (icyMetaInt > 0)) {
    int beforeIcy = icyMetaInt - icyByteCount;
    int ret = stream->read(reinterpret_cast<uint8_t*>(data), beforeIcy);
    read += ret;
    pos += ret;
    len -= ret;
    data = (void *)(reinterpret_cast<char*>(data) + ret);
    icyByteCount += ret;
    if (ret != beforeIcy) return read; // Partial read

    uint8_t mdSize;
    int mdret = stream->read(&mdSize, 1);
    if ((mdret == 1) && (mdSize > 0)) {
      ICYMDReader mdr(stream, mdSize * 16);
      // Break out (potentially multiple) NAME='xxxx'
      char type[32];
      char value[64];
      while (!mdr.eof()) {
        memset(type, 0, sizeof(type));
        memset(value, 0, sizeof(value));
        uint8_t c;
        char *p = type;
        for (size_t i=0; i<sizeof(type)-1; i++) {
          int ret = mdr.read(&c, 1);
          if (!ret) break;
          if (c == '=') break;
          *(p++) = c;
        }
        if (c != '=') {
          // MD type was too long, read remainder and throw away
          while ((c != '=') && !mdr.eof()) mdr.read(&c, 1);
        }
        mdr.read(&c, 1);
        if (c=='\'') {
          // Got start of string value, read that, too
          p = value;
          for (size_t i=0; i<sizeof(value)-1; i++) {
            int ret = mdr.read(&c, 1);
            if (!ret) break;
            if (c == '\'') {
              // Need to special case as you don't escape "'", so look for '; to terminate
              uint8_t d = ';'; // In case of EOF, return end of line
              mdr.read(&d, 1);
              if (d==';') { mdr.unread(d); break; }
              else { mdr.unread(d); }
            }
            *(p++) = c;
          }
          if (c != '\'') {
            // MD data was too long, read and throw away rest
            while ((c != '\'') && !mdr.eof()) mdr.read(&c, 1);
          }
          cb.md(type, false, value);
          do {
            ret = mdr.read(&c, 1);
          } while ((c !=';') && (c!=0) && !mdr.eof());
        }
      }
    }
    icyByteCount = 0;
  }

  int ret = stream->read(reinterpret_cast<uint8_t*>(data), len);
  read += ret;
  pos += ret;
  icyByteCount += ret;
  return read;
}
