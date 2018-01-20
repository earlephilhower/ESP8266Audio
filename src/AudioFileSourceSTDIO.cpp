/*
  AudioFileSourceSTDIO
  Input STDIO "file" to be used by AudioGenerator
  Only for hot-based testing
  
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
#ifndef ARDUINO

#include "AudioFileSourceSTDIO.h"

AudioFileSourceSTDIO::AudioFileSourceSTDIO()
{
  f = NULL;
}

AudioFileSourceSTDIO::AudioFileSourceSTDIO(const char *filename)
{
  open(filename);
}

bool AudioFileSourceSTDIO::open(const char *filename)
{
  f = fopen(filename, "rb");
  return f;
}

AudioFileSourceSTDIO::~AudioFileSourceSTDIO()
{
  if (f) fclose(f);
  f = NULL;
}

uint32_t AudioFileSourceSTDIO::read(void *data, uint32_t len)
{
  return fread(reinterpret_cast<uint8_t*>(data), 1, len, f);
}

bool AudioFileSourceSTDIO::seek(int32_t pos, int dir)
{
  return fseek(f, pos, dir);
}

bool AudioFileSourceSTDIO::close()
{
  fclose(f);
  f = NULL;
  return true;
}

bool AudioFileSourceSTDIO::isOpen()
{
  return f?true:false;
}

uint32_t AudioFileSourceSTDIO::getSize()
{
  if (!f) return 0;
  uint32_t p = ftell(f);
  fseek(f, 0, SEEK_END);
  uint32_t len = ftell(f);
  fseek(f, p, SEEK_SET);
  return len;
}


#endif
