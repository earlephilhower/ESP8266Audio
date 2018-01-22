#include <Arduino.h>
#include "AudioFileSourceSTDIO.h"
#include "AudioOutputSTDIO.h"
#include "AudioGeneratorAAC.h"

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    AudioFileSourceSTDIO *in = new AudioFileSourceSTDIO("jamonit.aac");
    AudioOutputSTDIO *out = new AudioOutputSTDIO();
    out->SetFilename("jamonit.aac.wav");
    void *space = malloc(28000+60000);
    AudioGeneratorAAC *aac = new AudioGeneratorAAC(space, 28000+60000);

    aac->begin(in, out);
    while (aac->loop()) { /*noop*/ }
    aac->stop();

    delete aac;
    delete out;
    delete in;

    free(space);
}
