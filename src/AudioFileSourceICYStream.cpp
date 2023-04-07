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

#if defined(ESP32) || defined(ESP8266)

#define _GNU_SOURCE

#include "AudioFileSourceICYStream.h"
#include <string.h>

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
  static const char *hdr[] = { "icy-metaint", "icy-name", "icy-genre", "icy-br", "Transfer-Encoding" };
  pos = 0;
  http.begin(client, url);
  http.addHeader("Icy-MetaData", "1");
  http.collectHeaders( hdr, 5 );
  http.setReuse(true);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
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
  if (http.hasHeader(hdr[1])) {
    String ret = http.header(hdr[1]);
//    cb.md("SiteName", false, ret.c_str());
  }
  if (http.hasHeader(hdr[2])) {
    String ret = http.header(hdr[2]);
//    cb.md("Genre", false, ret.c_str());
  }
  if (http.hasHeader(hdr[3])) {
    String ret = http.header(hdr[3]);
//    cb.md("Bitrate", false, ret.c_str());
  }
  
  if (http.hasHeader(hdr[4])) {
    String ret = http.header(hdr[4]);
	chunked = ret == "chunked";
  }

  metaLen = icyMetaInt;
  size = http.getSize();
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL)-1] = 0;
  return true;
}

AudioFileSourceICYStream::~AudioFileSourceICYStream()
{
  http.end();
}

// read stream while looking for a string with max length to read
bool findN(WiFiClient *stream, const char *txtIn, int txtInNum, int& maxBytes)
{
	const char *txt = txtIn;
	int txtNum = txtInNum;
	int c;
	while(maxBytes)
	{
		do{c = stream->read();}while(c==-1);
		maxBytes--;
		if (c == *txt)
		{
			txt++;
			txtNum--;
			if (txtNum==0)
				return true;
		}
		else
		{
			txtNum = txtInNum;
			txt = txtIn;
		}
	}
	return false;
}

// read hexa chunck size
uint32_t ReadChunkSize(WiFiClient *stream, bool first)
{
	char tmp[64];
	char* tmp2 = tmp;
	int c;
	if (!first)
	{
		do{c = stream->read();}while(c==-1); // remove \r
		do{c = stream->read();}while(c==-1); // remove \n
	}
	
	do
	{
		do{c = stream->read();}while(c==-1);
		if (c == '\r' || c == ';')
			break;
		*tmp2 = c; tmp2++;
	}
	while(tmp2-tmp < sizeof(tmp)-1);
	*tmp2 = 0;
	uint32_t ret = strtoul(tmp, nullptr, 16);
	do
	{
		do{c = stream->read();}while(c==-1);
	}
	while(c != '\n');
	return ret;
}

uint32_t AudioFileSourceICYStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
	if (size > 0)
	{
		if (pos >= size) return 0;
		if (len > (uint32_t)(pos - size))// Can't read past EOF...
			len = pos - size;
	}
	
	uint32_t initLen = len;
	WiFiClient *stream = http.getStreamPtr();
	uint8_t* cdata = reinterpret_cast<uint8_t*>(data);
	while(len)
	{
		if (chunked && chunckLen == 0)
			chunckLen = ReadChunkSize(stream, pos == 0);
		
		if (icyMetaInt > 0 && metaLen == 0)
		{
			metaLen = icyMetaInt;
			int c;
			do{c = stream->read();}while(c==-1);
			chunckLen--;
			if (c != 0)
			{
				int mdSize = c * 16;
				chunckLen -= mdSize;
				const char* txt = "StreamTitle=";
				if (findN(stream, txt, 12, mdSize))
				{
					char buffer[256];
					char *buffer2 = buffer;
					do{c = stream->read();}while(c==-1);
					mdSize--;
					if(c != '"' && c != '\'')
					{
						*buffer2 = c; buffer2++;
					}
					do
					{
						do{c = stream->read();}while(c==-1);
						mdSize--;
						if(c == '"' || c == '\'' || c==';' || c=='\0')
							break;
						*buffer2 = c; buffer2++;
					}while(mdSize && buffer2-buffer < sizeof(buffer)-1);
					*buffer2 = 0;
					cb.md("StreamTitle", false, buffer);
				}
				while(mdSize) // skip remaining metadata
				{
					do{c = stream->read();}while(c==-1);
					mdSize--;
				}
			}
		}
		int c;
		uint32_t avail = stream->available();
		if (!nonBlock && avail == 0) 
		{
			yield();
			continue;
		}
		uint32_t rsize = std::min<uint32_t>(len, avail);
		if (icyMetaInt > 0)
			rsize =  std::min(rsize, metaLen);
		if (chunked)
			rsize = std::min(rsize, chunckLen);
		
		if (rsize)
		{
			stream->read(cdata, rsize);
			len -= rsize;
			metaLen -= rsize;
			chunckLen -= rsize;
			pos += rsize;
			cdata += rsize;
		}		
	}
	return initLen;
}

#endif
