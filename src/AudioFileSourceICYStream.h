/*
  AudioFileSourceHTTPStream
  Connect to a HTTP based streaming service
  
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

#ifndef _AUDIOFILESOURCEICYSTREAM_H
#define _AUDIOFILESOURCEICYSTREAM_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>

#include "AudioFileSourceHTTPStream.h"

class AudioFileSourceICYStream : public AudioFileSourceHTTPStream
{
  public:
    AudioFileSourceICYStream();
    AudioFileSourceICYStream(const char *url);
    virtual ~AudioFileSourceICYStream() override;
    
    virtual bool open(const char *url) override;
//    virtual uint32_t read(void *data, uint32_t len) override;
//    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
//    virtual bool seek(int32_t pos, int dir) override;
//    virtual bool close() override;
//    virtual bool isOpen() override;
//    virtual uint32_t getSize() override;
//    virtual uint32_t getPos() override;
//    bool SetReconnect(bool val) { reconnect = val; return true; }

  private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock) override;
    int icyMetaInt;
    int icyByteCount;
};


#endif

