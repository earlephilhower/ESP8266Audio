#include <Arduino.h>
#include "AudioFileSourceSTDIO.h"
#include "AudioOutputSTDIO.h"
#include "AudioGeneratorMP3.h"

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    AudioFileSourceSTDIO *in = new AudioFileSourceSTDIO("jamonit.mp3");
    AudioOutputSTDIO *out = new AudioOutputSTDIO();
    out->SetFilename("jamonit.wav");
    AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();

    mp3->begin(in, out);
    while (mp3->loop()) { /*noop*/ }
    mp3->stop();
}
