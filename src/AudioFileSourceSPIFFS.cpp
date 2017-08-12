/*
  AudioFileSourceSPIFFS
  Input SPIFFS "file" to be used by AudioGenerator
  
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

#include "AudioFileSourceSPIFFS.h"

AudioFileSourceSPIFFS::AudioFileSourceSPIFFS()
{
}

AudioFileSourceSPIFFS::AudioFileSourceSPIFFS(const char *filename)
{
  open(filename);
}

bool AudioFileSourceSPIFFS::open(const char *filename)
{
  SPIFFS.begin();
  f = SPIFFS.open(filename, "r");
  return f;
}

AudioFileSourceSPIFFS::~AudioFileSourceSPIFFS()
{
  if (f) f.close();
}

uint32_t AudioFileSourceSPIFFS::read(void *data, uint32_t len)
{
  return f.read(reinterpret_cast<uint8_t*>(data), len);
}

bool AudioFileSourceSPIFFS::seek(int32_t pos, int dir)
{
  return f.seek(pos, (dir==SEEK_SET)?SeekSet:(dir==SEEK_CUR)?SeekCur:SeekEnd);
}

bool AudioFileSourceSPIFFS::close()
{
  f.close();
  return true;
}

bool AudioFileSourceSPIFFS::isOpen()
{
  return f?true:false;
}

uint32_t AudioFileSourceSPIFFS::getSize()
{
  if (!f) return 0;
  return f.size();
}


