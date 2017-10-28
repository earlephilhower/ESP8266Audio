/*
  AudioFileSourceFastROMFS
  Input FastROMFS "file" to be used by AudioGenerator
  
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

#include "AudioFileSourceFastROMFS.h"

AudioFileSourceFastROMFS::AudioFileSourceFastROMFS()
{

}

AudioFileSourceFastROMFS::AudioFileSourceFastROMFS(FastROMFilesystem *fs, const char *filename)
{
  this->fs = fs;
  open(filename);
}

bool AudioFileSourceFastROMFS::open(const char *filename)
{
  f = fs->open(filename, "r");
  return f;
}

AudioFileSourceFastROMFS::~AudioFileSourceFastROMFS()
{
  if (f) f->close();
  f = NULL;
}

uint32_t AudioFileSourceFastROMFS::read(void *data, uint32_t len)
{
  return f->read(reinterpret_cast<uint8_t*>(data), len);
}

bool AudioFileSourceFastROMFS::seek(int32_t pos, int dir)
{
  return f->seek(pos, dir);
}

bool AudioFileSourceFastROMFS::close()
{
  f->close();
  return true;
}

bool AudioFileSourceFastROMFS::isOpen()
{
  return f?true:false;
}

uint32_t AudioFileSourceFastROMFS::getSize()
{
  if (!f) return 0;
  return f->size();
}


