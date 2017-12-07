/*
  AudioFileSourceID3
  ID3 filter that extracts any ID3 fields and sends to CB function
  
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

#include "AudioFileSourceID3.h"
#include "AudioFileStream.h"

// Handle unsync operation in ID3 with custom class
class AudioFileSourceUnsync : public AudioFileSource
{
  public:
    AudioFileSourceUnsync(AudioFileSource *src, int len, bool unsync);
    virtual ~AudioFileSourceUnsync() override;
    virtual uint32_t read(void *data, uint32_t len) override;

    int getByte();
    bool eof();

  private:
    AudioFileSource *src;
    int remaining;
    bool unsync;
    int savedByte;
};

AudioFileSourceUnsync::AudioFileSourceUnsync(AudioFileSource *src, int len, bool unsync)
{
  this->src = src;
  this->remaining = len;
  this->unsync = unsync;
  this->savedByte = -1;
}

AudioFileSourceUnsync::~AudioFileSourceUnsync()
{
}

uint32_t AudioFileSourceUnsync::read(void *data, uint32_t len)
{
  uint32_t bytes = 0;
  uint8_t *ptr = reinterpret_cast<uint8_t*>(data);

  // This is only used during ID3 parsing, so no need to optimize here...
  while (len--) {
    int b = getByte();
    if (b >= 0) {
      *(ptr++) = (uint8_t)b;
      bytes++;
    }
  }
  return bytes;
}

int AudioFileSourceUnsync::getByte()
{
  // If we've saved a pre-read character, return it immediately
  if (savedByte >= 0) {
    int s = savedByte;
    savedByte = -1;
    return s;
  }

  if (remaining <= 0) {
    return -1;
  } else if (remaining == 1) {
    remaining--;
    uint8_t c;
    if (1 != src->read(&c, 1)) return -1;
    else return c;
  } else {
    uint8_t c;
    remaining--;
    if (1 != src->read(&c, 1)) return -1;
    if (c != 0xff) {
      return c;
    }
    // Saw 0xff, check next byte.  If 0 then eat it, OTW return the 0xff
    uint8_t d;
    remaining--;
    if (1 != src->read(&d, 1)) return c;
    if (d != 0x00) {
      savedByte = d;
    }
    return c;
  }
}

bool AudioFileSourceUnsync::eof()
{
  if (remaining<=0) return true;
  else return false;
}



AudioFileSourceID3::AudioFileSourceID3(AudioFileSource *src)
{
  this->src = src;
  this->checked = false;
}

AudioFileSourceID3::~AudioFileSourceID3()
{
  src->close();
}

uint32_t AudioFileSourceID3::read(void *data, uint32_t len)
{
  if (checked) {
    return src->read(data, len);
  }
  checked = true;
  // <10 bytes initial read, not enough space to check header
  if (len<10) return src->read(data, len);

  uint8_t *buff = reinterpret_cast<uint8_t*>(data);
  int ret = src->read(data, 10);
  if (ret<10) return ret;

  if ((buff[0]!='I') || (buff[1]!='D') || (buff[2]!='3') || (buff[3]>0x04) || (buff[4]!=0)) {
    return 10 + src->read(buff+10, len-10);
  }

  bool unsync = (buff[5] & 0x80);
  bool exthdr = (buff[5] & 0x40);
  int id3Size = buff[6];
  id3Size = id3Size << 7;
  id3Size |= buff[7];
  id3Size = id3Size << 7;
  id3Size |= buff[8];
  id3Size = id3Size << 7;
  id3Size |= buff[9];
  AudioFileSourceUnsync id3(src, id3Size-10, unsync);
  // Every read from now may be unsync'd

  if (exthdr) {
    int ehsz = (id3.getByte()<<24) | (id3.getByte()<<16) | (id3.getByte()<<8) | (id3.getByte());
    for (int j=0; j<ehsz; j++) id3.getByte(); // Throw it away
  }

  do {
    unsigned char frameid[4];
    int framesize;
    bool compressed;
    frameid[0] = id3.getByte();
    frameid[1] = id3.getByte();
    frameid[2] = id3.getByte();
    frameid[3] = id3.getByte();
    if (frameid[0]==0 && frameid[1]==0 && frameid[2]==0 && frameid[3]==0) {
      // We're in padding
      while (!id3.eof()) {
        id3.getByte();
      }
    } else {
      framesize = (id3.getByte()<<24) | (id3.getByte()<<16) | (id3.getByte()<<8) | (id3.getByte());
      id3.getByte(); // skip 1st flag
      compressed = id3.getByte()&0x80;
      if (compressed) {
        int decompsize = (id3.getByte()<<24) | (id3.getByte()<<16) | (id3.getByte()<<8) | (id3.getByte());
        // TODO - add libz decompression, for now ignore this one...
        (void)decompsize;
        for (int j=0; j<framesize; j++)
          id3.getByte();
      }
      {
        AudioFileStream afs(&id3, framesize);
        if (frameid[0]=='T' && frameid[1]=='A' && frameid[2]=='L' && frameid[3] == 'B') {
          cb.md("Album", afs.read()==1, &afs);
        } else if (frameid[0]=='T' && frameid[1]=='I' && frameid[2]=='T' && frameid[3] == '2') {
          cb.md("Title", afs.read()==1, &afs);
       } else if (frameid[0]=='T' && frameid[1]=='P' && frameid[2]=='E' && frameid[3] == '1') {
          cb.md("Performer", afs.read()==1, &afs);
        } else if (frameid[0]=='T' && frameid[1]=='Y' && frameid[2]=='E' && frameid[3] == 'R') {
          cb.md("Year", afs.read()==1, &afs);
        }
        //afs destructor here will read any unread bytes
      }
    }
  } while (!id3.eof());

  // All ID3 processing done, return to main caller
  return src->read(data, len);
}

bool AudioFileSourceID3::seek(int32_t pos, int dir)
{
  return src->seek(pos, dir);
}

bool AudioFileSourceID3::close()
{
  return src->close();
}

bool AudioFileSourceID3::isOpen()
{
  return src->isOpen();
}

uint32_t AudioFileSourceID3::getSize()
{
  return src->getSize();
}

uint32_t AudioFileSourceID3::getPos()
{
  return src->getPos();
}
