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

#ifndef _AUDIOFILESOURCEHTTPSTREAM_H
#define _AUDIOFILESOURCEHTTPSTREAM_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>

#include "AudioFileSource.h"

class AudioFileSourceHTTPStream : public AudioFileSource
{
  public:
    AudioFileSourceHTTPStream();
    AudioFileSourceHTTPStream(const char *url);
    virtual ~AudioFileSourceHTTPStream() override;
    
    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;

  private:
    HTTPClient http;
    int pos;
    int size;
};


#endif

