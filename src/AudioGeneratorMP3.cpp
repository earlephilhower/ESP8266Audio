/*
  AudioGeneratorMP3
  Wrap libmad MP3 library to play audio
  
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


#include "AudioGeneratorMP3.h"

AudioGeneratorMP3::AudioGeneratorMP3()
{
  running = false;
  file = NULL;
  output = NULL;
  buffLen = 1500; // Max theoretical frame of 1441 + 8 guard bytes, with some fluff
  buff = NULL;
  nsCountMax = 1152/32;
}

AudioGeneratorMP3::~AudioGeneratorMP3()
{
  free(buff);
  buff = NULL;
}

bool AudioGeneratorMP3::SetBufferSize(int sz)
{
  if (running) return false;
  buffLen = sz;
  return true;
}


bool AudioGeneratorMP3::stop()
{
  free(buff);
  buff = NULL;
  
  mad_synth_finish(&synth);
  mad_frame_finish(&frame);
  mad_stream_finish(&stream);

  running = false;
  output->stop();
  return file->close();
}

bool AudioGeneratorMP3::isRunning()
{
  return running;
}

enum mad_flow AudioGeneratorMP3::ErrorToFlow()
{
  char err[64];
  char errLine[128];

  // Special case - eat "lost sync @ byte 0" as it always occurs and is not really correct....it never had sync!
  if ((lastReadPos==0) && (stream.error==MAD_ERROR_LOSTSYNC)) return MAD_FLOW_CONTINUE;

  strcpy_P(err, mad_stream_errorstr(&stream));
  snprintf_P(errLine, sizeof(errLine), PSTR("Decoding error '%s' at byte offset %d"),
           err, (stream.this_frame - buff) + lastReadPos);
  yield(); // Something bad happened anyway, ensure WiFi gets some time, too
  cb.st(stream.error, errLine);
  return MAD_FLOW_CONTINUE;
}

enum mad_flow AudioGeneratorMP3::Input()
{
  int unused = 0;

  if (stream.next_frame) {
    unused = buffLen - (stream.next_frame - buff);
    memmove(buff, stream.next_frame, unused);
    stream.next_frame = NULL;
  }
  if (unused == buffLen) {
    // Something wicked this way came, throw it all out and try again
    unused = 0;
  }

  lastReadPos = file->getPos() - unused;
  int len = buffLen - unused;
  len = file->read(buff + unused, len);
  if (len == 0) {
    Serial.printf_P(PSTR("MP3 stop, len==0\n"));
    return MAD_FLOW_STOP;
  }

  mad_stream_buffer(&stream, buff, len + unused);

  return MAD_FLOW_CONTINUE;
}


bool AudioGeneratorMP3::DecodeNextFrame()
{
  if (mad_frame_decode(&frame, &stream) == -1) {
    ErrorToFlow(); // Always returns CONTINUE
    return false;
  }
  nsCountMax  = MAD_NSBSAMPLES(&frame.header);
  return true;
}

bool AudioGeneratorMP3::GetOneSample(int16_t sample[2])
{
  if (synth.pcm.samplerate != lastRate) {
    output->SetRate(synth.pcm.samplerate);
    lastRate = synth.pcm.samplerate;
  }
  if (synth.pcm.channels != lastChannels) {
    output->SetChannels(synth.pcm.channels);
    lastChannels = synth.pcm.channels;
  }
    
  // If we're here, we have one decoded frame and sent 0 or more samples out
  if (samplePtr < synth.pcm.length) {
    sample[AudioOutput::LEFTCHANNEL ] = synth.pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth.pcm.samples[1][samplePtr];
    samplePtr++;
  } else {
    samplePtr = 0;
    
    switch ( mad_synth_frame_onens(&synth, &frame, nsCount++) ) {
        case MAD_FLOW_STOP:
        case MAD_FLOW_BREAK: Serial.printf_P(PSTR("msf1ns failed\n"));
          return false; // Either way we're done
        default:
          break; // Do nothing
    }
    // for IGNORE and CONTINUE, just play what we have now
    sample[AudioOutput::LEFTCHANNEL ] = synth.pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth.pcm.samples[1][samplePtr];
    samplePtr++;
  }
  return true;
}


bool AudioGeneratorMP3::loop()
{
  if (!running) goto done; // Nothing to do here!

  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Decode next frame if we're beyond the existing generated data
    if ( (samplePtr >= synth.pcm.length) && (nsCount >= nsCountMax) ) {
retry:
      if (Input() == MAD_FLOW_STOP) {
        return false;
      }

      if (!DecodeNextFrame()) {
        goto retry;
      }
      samplePtr = 9999;
      nsCount = 0;
    }

    if (!GetOneSample(lastSample)) {
      Serial.printf_P(PSTR("G1S failed\n"));
      running = false;
      goto done;
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}



bool AudioGeneratorMP3::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source)  return false;
  file = source;
  if (!output) return false;
  this->output = output;
  if (!file->isOpen()) {
    Serial.printf_P(PSTR("MP3 source file not open\n"));
    return false; // Error
  }
  if (!output->begin()) return false;

  // Where we are in generating one frame's data, set to invalid so we will run loop on first getsample()
  samplePtr = 9999;
  nsCount = 9999;
  synth.pcm.length = 0;
  lastRate = 0;
  lastChannels = 0;
  lastReadPos = 0;
  buff = reinterpret_cast<unsigned char *>(malloc(buffLen));
  if (!buff) return false;
  
  mad_stream_init(&stream);
  mad_frame_init(&frame);
  mad_synth_init(&synth);

  mad_stream_options(&stream, 0); // TODO - add options suppoirt
 
  running = true;
  return true;
}

// The following are helper routines for use in libmad to check stack/heap free
// and to determine if there's enough stack space to allocate some blocks there
// instead of precious heap.

#undef stack
extern "C" {
#ifdef ESP32
  //TOTO - add ESP32 checks
  void stack(const char *s, const char *t, int i)
  {
  }
  int stackfree()
  {
    return 8192;
  }
#else
  #include <cont.h>
  extern cont_t g_cont;

  void stack(const char *s, const char *t, int i)
  {
    (void) t;
    (void) i;
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    int freeheap = ESP.getFreeHeap();
    if ((freestack < 512) || (freeheap < 5120)) {
      static int laststack, lastheap;
      if (laststack!=freestack|| lastheap !=freeheap) {
        Serial.printf_P(PSTR("%s: FREESTACK=%d, FREEHEAP=%d\n"), s, /*t, i,*/ freestack, /*cont_get_free_stack(&g_cont),*/ freeheap);
      }
      if (freestack < 256) {
        Serial.printf_P(PSTR("out of stack!\n"));
      }
      if (freeheap < 1024) {
        Serial.printf_P(PSTR("out of heap!\n"));
      }
      Serial.flush();
      laststack = freestack;
      lastheap = freeheap;
    }
  }

  int stackfree()
  {
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    return freestack;
  }
#endif  
}

