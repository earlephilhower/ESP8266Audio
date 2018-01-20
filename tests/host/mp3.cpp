#include <Arduino.h>
#include "AudioFileSourceSTDIO.h"
#include "AudioOutputSTDIO.h"
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceID3.h"

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  (void)cbData;
  Serial.printf("ID3 callback for: %s = '", type);

  if (isUnicode) {
    string += 2;
  }
  
  while (*string) {
    char a = *(string++);
    if (isUnicode) {
      string++;
    }
    Serial.printf("%c", a);
  }
  Serial.printf("'\n");
  Serial.flush();
}



int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    AudioFileSourceSTDIO *in = new AudioFileSourceSTDIO("jamonit.mp3");
    AudioFileSourceID3 *id3 = new AudioFileSourceID3(in);
    id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
    AudioOutputSTDIO *out = new AudioOutputSTDIO();
    out->SetFilename("jamonit.wav");
    AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();

    mp3->begin(id3, out);
    while (mp3->loop()) { /*noop*/ }
    mp3->stop();

    delete mp3;
    delete out;
    delete id3;
    delete in;
}
