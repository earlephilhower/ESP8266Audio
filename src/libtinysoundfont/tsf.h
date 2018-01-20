/* TinySoundFont - v0.7 - SoundFont2 synthesizer - https://github.com/schellingb/TinySoundFont
                                     no warranty implied; use at your own risk
   Do this:
      #define TSF_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #define TSF_IMPLEMENTATION
   #include "tsf.h"

   [OPTIONAL] #define TSF_NO_STDIO to remove stdio dependency
   [OPTIONAL] #define TSF_MALLOC, TSF_REALLOC, and TSF_FREE to avoid stdlib.h
   [OPTIONAL] #define TSF_MEMCPY, TSF_MEMSET to avoid string.h
   [OPTIONAL] #define TSF_POW, TSF_POWF, TSF_EXPF, TSF_LOG, TSF_TAN, TSF_LOG10, TSF_SQRT to avoid math.h

   NOT YET IMPLEMENTED
     - Lower level voice interface to render single voices/presets
     - Support for ChorusEffectsSend and ReverbEffectsSend generators
     - Better low-pass filter without lowering performance too much
     - Support for modulators

   LICENSE (MIT)

   Copyright (C) 2017 Bernhard Schelling
   Based on SFZero, Copyright (C) 2012 Steve Folta (https://github.com/stevefolta/SFZero)

   Permission is hereby granted, free of charge, to any person obtaining a copy of this
   software and associated documentation files (the "Software"), to deal in the Software
   without restriction, including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
   to whom the Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef TSF_INCLUDE_TSF_INL
#define TSF_INCLUDE_TSF_INL

#ifdef __cplusplus
extern "C" {
#  define CPP_DEFAULT0 = 0
#else
#  define CPP_DEFAULT0
#endif

//define this if you want the API functions to be static
#ifdef TSF_STATIC
#define TSFDEF static
#else
#define TSFDEF extern
#endif

// The load functions will return a pointer to a struct tsf which all functions
// thereafter take as the first parameter.
// On error the tsf_load* functions will return NULL most likely due to invalid
// data (or if the file did not exist in tsf_load_filename).
typedef struct tsf tsf;

#ifndef TSF_NO_STDIO
// Directly load a SoundFont from a .sf2 file path
TSFDEF tsf* tsf_load_filename(const char* filename);
#endif

// Load a SoundFont from a block of memory
TSFDEF tsf* tsf_load_memory(const void* buffer, int size);

// Stream structure for the generic loading
struct tsf_stream
{
	// Custom data given to the functions as the first parameter
	void* data;

	// Function pointer will be called to read 'size' bytes into ptr
	int (*read)(void* data, void* ptr, unsigned int size);

	// Function pointer will be called to skip ahead over 'count' bytes
	int (*tell)(void* data);

	// Function pointer will be called to skip ahead over 'count' bytes
	int (*skip)(void* data, unsigned int count);

	// Function pointer will be called to seek to absolute position
	int (*seek)(void* data, unsigned int pos);

	// Function pointer will be called to skip ahead over 'count' bytes
	int (*close)(void* data);
	
	// Function pointer will be called to skip ahead over 'count' bytes
	int (*size)(void* data);
};

// Generic SoundFont loading method using the stream structure above
TSFDEF tsf* tsf_load(struct tsf_stream* stream);

// Free the memory related to this tsf instance
TSFDEF void tsf_close(tsf* f);

// Returns the number of presets in the loaded SoundFont
TSFDEF int tsf_get_presetcount(tsf* f);

// Returns the name of a preset index >= 0 and < tsf_get_presetcount()
TSFDEF const char* tsf_get_presetname(tsf* f, int preset);

// Supported output modes by the render methods
enum TSFOutputMode
{
	// Two channels with single left/right samples one after another
	TSF_STEREO_INTERLEAVED,
	// Two channels with all samples for the left channel first then right
	TSF_STEREO_UNWEAVED,
	// A single channel (stereo instruments are mixed into center)
	TSF_MONO
};

// Thread safety:
// Your audio output which calls the tsf_render* functions will most likely
// run on a different thread than where the playback tsf_note* functions
// are called. In which case some sort of concurrency control like a
// mutex needs to be used so they are not called at the same time.

// Setup the parameters for the voice render methods
//   outputmode: if mono or stereo and how stereo channel data is ordered
//   samplerate: the number of samples per second (output frequency)
//   globalgaindb: volume gain in decibels (>0 means higher, <0 means lower)
TSFDEF void tsf_set_output(tsf* f, enum TSFOutputMode outputmode, int samplerate, float globalgaindb CPP_DEFAULT0);

// Start playing a note
//   preset: preset index >= 0 and < tsf_get_presetcount()
//   key: note value between 0 and 127 (60 being middle C)
//   vel: velocity as a float between 0.0 (equal to note off) and 1.0 (full)
TSFDEF void tsf_note_on(tsf* f, int preset, int key, float vel);

// Stop playing a note
TSFDEF void tsf_note_off(tsf* f, int preset, int key);

// Render output samples into a buffer
// You can either render as signed 16-bit values (tsf_render_short) or
// as 32-bit float values (tsf_render_float)
//   buffer: target buffer of size samples * output_channels * sizeof(type)
//   samples: number of samples to render
//   flag_mixing: if 0 clear the buffer first, otherwise mix into existing data
TSFDEF void tsf_render_short(tsf* f, short* buffer, int samples, int flag_mixing CPP_DEFAULT0);
TSFDEF void tsf_render_float(tsf* f, float* buffer, int samples, int flag_mixing CPP_DEFAULT0);

#ifdef __cplusplus
#  undef CPP_DEFAULT0
}
#endif

// end header
// ---------------------------------------------------------------------------------------------------------
#endif //TSF_INCLUDE_TSF_INL

#ifdef TSF_IMPLEMENTATION

// The lower this block size is the more accurate the effects are.
// Increasing the value significantly lowers the CPU usage of the voice rendering.
// If LFO affects the low-pass filter it can be hearable even as low as 8.
#ifndef TSF_RENDER_EFFECTSAMPLEBLOCK
#define TSF_RENDER_EFFECTSAMPLEBLOCK 64
#endif

// Grace release time for quick voice off (avoid clicking noise)
#define TSF_FASTRELEASETIME 0.01f
#if !defined(TSF_MALLOC) || !defined(TSF_FREE) || !defined(TSF_REALLOC)
#  include <stdlib.h>
#  define TSF_MALLOC  malloc
#  define TSF_FREE    free
#  define TSF_REALLOC realloc
#endif

#if !defined(TSF_MEMCPY) || !defined(TSF_MEMSET)
#  include <string.h>
#  define TSF_MEMCPY  memcpy
#  define TSF_MEMSET  memset
#endif

#if !defined(TSF_POW) || !defined(TSF_POWF) || !defined(TSF_EXPF) || !defined(TSF_LOG) || !defined(TSF_TAN) || !defined(TSF_LOG10) || !defined(TSF_SQRT)
#  include <math.h>
#  if !defined(__cplusplus) && !defined(NAN) && !defined(powf) && !defined(expf)
#    define powf (float)pow // deal with old math.h files that
#    define expf (float)exp // come without powf and expf
#  endif
#  define TSF_POW     pow
#  define TSF_POWF    powf
#  define TSF_EXPF    expf
#  define TSF_LOG     log
#  define TSF_TAN     tan
#  define TSF_LOG10   log10
#  define TSF_SQRT    sqrt
#endif

#ifndef TSF_NO_STDIO
#  include <stdio.h>
#endif

#define TSF_TRUE 1
#define TSF_FALSE 0
#define TSF_BOOL char
#define TSF_PI 3.14159265358979323846264338327950288
#define TSF_NULL 0

#ifdef __cplusplus
extern "C" {
#endif

typedef char tsf_fourcc[4];
typedef signed char tsf_s8;
typedef unsigned char tsf_u8;
typedef unsigned short tsf_u16;
typedef signed short tsf_s16;
typedef unsigned int tsf_u32;
typedef char tsf_char20[20];

#define TSF_FourCCEquals(value1, value2) (value1[0] == value2[0] && value1[1] == value2[1] && value1[2] == value2[2] && value1[3] == value2[3])

// Samples cache, number and sample count
#define TSF_BUFFS 16
#define TSF_BUFFSIZE 512

struct tsf
{
	struct tsf_preset* presets;
	int presetNum;

	int fontSamplesOffset;
	int fontSampleCount;

	struct tsf_voice *voices;
	int voiceNum;

	float outSampleRate;
	enum TSFOutputMode outputmode;
	float globalGainDB;

	float* outputSamples;
	int outputSampleSize;

	struct tsf_hydra *hydra;

	// Cached sample read 
	short *buffer[TSF_BUFFS];
	int offset[TSF_BUFFS];
	int timestamp[TSF_BUFFS];
	int epoch;
};

struct tsf_stream_cached_data {
	int buffs;
	int buffsize;
	struct tsf_stream *stream;
	unsigned int size, pos;
	unsigned char **buffer;
	unsigned int *offset;
	unsigned int *timestamp;
	unsigned int epoch;
	unsigned int hit;
	unsigned int miss;
};

static int tsf_stream_cached_read(void* v, void* ptr, unsigned int size)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	unsigned char *p = (unsigned char *)ptr;

	while (size) {
		if (d->pos >= d->size) return 0; // EOF
		for (int i=0; i < d->buffs; i++) {
			if ((d->offset[i] <= d->pos) && ((d->offset[i] + d->buffsize) > d->pos) ) {
				d->timestamp[i] = d->epoch++;
				// Handle case of epoch rollover by just setting low, random epochs
				if (d->epoch==0) {
					for (int i=0; i<d->buffs; i++) d->timestamp[i] = d->epoch++;
				}
				unsigned int startOffset = d->pos - d->offset[i];
				unsigned int len = d->buffsize - (d->pos % d->buffsize);
				if (len > size) len = size;
				TSF_MEMCPY(p, &d->buffer[i][startOffset], len);
				size -= len;
				d->pos += len;
				p += len;
				d->hit++;
				if (size == 0) return 1;
				i = -1; // Restart the for() at block 0 after postincrement
			}
		}
		
		int repl = 0;
		for (int i=1; i < d->buffs; i++) {
			if (d->timestamp[i] < d->timestamp[repl]) repl = i;
		}
		int readOff = d->pos - (d->pos % d->buffsize);
		d->stream->seek(d->stream->data, readOff);
		d->stream->read(d->stream->data, d->buffer[repl], d->buffsize);
		d->timestamp[repl] = d->epoch++;
		d->offset[repl] = readOff;
		d->miss++;
		d->hit--; // Avoid counting this as a hit on next loop that returns data
		// Don't actually do anything yet, we'll retry the search on next loop where it will notice new data
	}
	return 1;
}

static int tsf_stream_cached_tell(void* v)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	return d->pos;
}

static int tsf_stream_cached_size(void* v)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	return d->size;
}

static int tsf_stream_cached_skip(void* v, unsigned int count)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	if ((d->pos + count) < d->size) {
		d->pos += count;
		return 1;
	}
	return 0;
}

static int tsf_stream_cached_seek(void* v, unsigned int pos)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	if (pos < d->size) {
		d->pos = pos;
		return 1;
	}
	return 0;
}

static int tsf_stream_cached_close(void* v)
{
	struct tsf_stream_cached_data *d = (struct tsf_stream_cached_data*)v;
	free(d->timestamp);
	free(d->offset);
	for (int i = d->buffs - 1; i >=0; i--) free(d->buffer[i]);
	free(d->buffer);
	int ret = d->stream->close(d->stream->data);
	free(d);
	return ret;
}

/* Wraps an existing stream with a caching layer.  First create the stream you need, then
   call this to create a new stream.  Use this new stream only, and when done close() will
   close both this stream as well as the wrapped one. */
TSFDEF struct tsf_stream *tsf_stream_wrap_cached(struct tsf_stream *stream, int buffs, int buffsize, struct tsf_stream *dest) 
{
	struct tsf_stream_cached_data *s = (struct tsf_stream_cached_data*)TSF_MALLOC(sizeof(*s));
	s->buffs  = buffs;
	s->buffsize = buffsize;
	s->stream = stream;
	s->size = stream->size(stream->data);
	s->pos = stream->tell(stream->data);
	s->buffer = (unsigned char **)TSF_MALLOC(sizeof(*s->buffer) * buffs);
	s->offset = (unsigned int *)TSF_MALLOC(sizeof(s->offset) * buffs);
	s->timestamp = (unsigned int *)TSF_MALLOC(sizeof(s->timestamp) * buffs);
	s->epoch = 0;
	s->hit = 0;
	s->miss = 0;
	for (int i=0; i<buffs; i++) {
		s->buffer[i] = (unsigned char *)TSF_MALLOC(buffsize);
		s->offset[i] = 0xfffffff;
		s->timestamp[i] = 0;
	}
	dest->data = (void*)s;
	dest->read = &tsf_stream_cached_read;
	dest->tell = &tsf_stream_cached_tell;
	dest->skip = &tsf_stream_cached_skip;
	dest->seek = &tsf_stream_cached_seek;
	dest->size = &tsf_stream_cached_size;
	dest->close = &tsf_stream_cached_close;
	return dest;
}


#ifndef TSF_NO_STDIO
static int tsf_stream_stdio_read(FILE* f, void* ptr, unsigned int size) { return (int)fread(ptr, 1, size, f); }
static int tsf_stream_stdio_tell(FILE* f) { return ftell(f); }
static int tsf_stream_stdio_size(FILE* f) { int p = ftell(f); fseek(f, 0, SEEK_END); int e = ftell(f); fseek(f, p, SEEK_SET); return e; }
static int tsf_stream_stdio_skip(FILE* f, unsigned int count) { return !fseek(f, count, SEEK_CUR); }
static int tsf_stream_stdio_seek(FILE* f, unsigned int count) { return !fseek(f, count, SEEK_SET); }
static int tsf_stream_stdio_close(FILE* f) { return !fclose(f); }
TSFDEF tsf* tsf_load_filename(const char* filename)
{
	tsf* res;
	struct tsf_stream stream = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_stdio_read, (int(*)(void*))&tsf_stream_stdio_tell, (int(*)(void*,unsigned int))&tsf_stream_stdio_skip, (int(*)(void*,unsigned int))&tsf_stream_stdio_seek, (int(*)(void*))&tsf_stream_stdio_close, (int(*)(void*))&tsf_stream_stdio_size };
	#if __STDC_WANT_SECURE_LIB__
	FILE* f = TSF_NULL; fopen_s(&f, filename, "rb");
	#else
	FILE* f = fopen(filename, "rb");
	#endif
	if (!f)
	{
		//if (e) *e = TSF_FILENOTFOUND;
		return TSF_NULL;
	}
	stream.data = f;
	res = tsf_load(&stream);
	//fclose(f);
	return res;
}
#endif

struct tsf_stream_memory { const char* buffer; unsigned int total, pos; };
static int tsf_stream_memory_read(struct tsf_stream_memory* m, void* ptr, unsigned int size) { if (size > m->total - m->pos) size = m->total - m->pos; TSF_MEMCPY(ptr, m->buffer+m->pos, size); m->pos += size; return size; }
static int tsf_stream_memory_tell(struct tsf_stream_memory* m) { return m->pos; }
static int tsf_stream_memory_size(struct tsf_stream_memory* m) { return m->total; }
static int tsf_stream_memory_skip(struct tsf_stream_memory* m, unsigned int count) { if (m->pos + count > m->total) return 0; m->pos += count; return 1; }
static int tsf_stream_memory_seek(struct tsf_stream_memory* m, unsigned int pos) { if (pos > m->total) return 0; else m->pos = pos; return 1; }
static int tsf_stream_memory_close(struct tsf_stream_memory* m) { (void)m; return 1; }
TSFDEF tsf* tsf_load_memory(const void* buffer, int size)
{
	struct tsf_stream stream = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_memory_read, (int(*)(void*))&tsf_stream_memory_tell, (int(*)(void*,unsigned int))&tsf_stream_memory_skip, (int(*)(void*,unsigned int))&tsf_stream_memory_seek, (int(*)(void*))&tsf_stream_memory_close, (int(*)(void*))&tsf_stream_memory_size };
	struct tsf_stream_memory f = { 0, 0, 0 };
	f.buffer = (const char*)buffer;
	f.total = size;
	stream.data = &f;
	return tsf_load(&stream);
}

enum { TSF_LOOPMODE_NONE, TSF_LOOPMODE_CONTINUOUS, TSF_LOOPMODE_SUSTAIN };

enum { TSF_SEGMENT_NONE, TSF_SEGMENT_DELAY, TSF_SEGMENT_ATTACK, TSF_SEGMENT_HOLD, TSF_SEGMENT_DECAY, TSF_SEGMENT_SUSTAIN, TSF_SEGMENT_RELEASE, TSF_SEGMENT_DONE };

struct tsf_hydra
{
	struct tsf_stream *stream;
	int phdrOffset, pbagOffset, pmodOffset, pgenOffset, instOffset, ibagOffset, imodOffset, igenOffset, shdrOffset;
	int phdrNum, pbagNum, pmodNum, pgenNum, instNum, ibagNum, imodNum, igenNum, shdrNum;
};

union tsf_hydra_genamount { struct { tsf_u8 lo, hi; } range; tsf_s16 shortAmount; tsf_u16 wordAmount; };
struct tsf_hydra_phdr { tsf_char20 presetName; tsf_u16 preset, bank, presetBagNdx; tsf_u32 library, genre, morphology; };
struct tsf_hydra_pbag { tsf_u16 genNdx, modNdx; };
struct tsf_hydra_pmod { tsf_u16 modSrcOper, modDestOper; tsf_s16 modAmount; tsf_u16 modAmtSrcOper, modTransOper; };
struct tsf_hydra_pgen { tsf_u16 genOper; union tsf_hydra_genamount genAmount; };
struct tsf_hydra_inst { tsf_char20 instName; tsf_u16 instBagNdx; };
struct tsf_hydra_ibag { tsf_u16 instGenNdx, instModNdx; };
struct tsf_hydra_imod { tsf_u16 modSrcOper, modDestOper; tsf_s16 modAmount; tsf_u16 modAmtSrcOper, modTransOper; };
struct tsf_hydra_igen { tsf_u16 genOper; union tsf_hydra_genamount genAmount; };
struct tsf_hydra_shdr { tsf_char20 sampleName; tsf_u32 start, end, startLoop, endLoop, sampleRate; tsf_u8 originalPitch; tsf_s8 pitchCorrection; tsf_u16 sampleLink, sampleType; };

#define TSFR(FIELD) stream->read(stream->data, &i->FIELD, sizeof(i->FIELD));
static void tsf_hydra_read_phdr(struct tsf_hydra_phdr* i, struct tsf_stream* stream) { TSFR(presetName) TSFR(preset) TSFR(bank) TSFR(presetBagNdx) TSFR(library) TSFR(genre) TSFR(morphology) }
static void tsf_hydra_read_pbag(struct tsf_hydra_pbag* i, struct tsf_stream* stream) { TSFR(genNdx) TSFR(modNdx) }
//static void tsf_hydra_read_pmod(struct tsf_hydra_pmod* i, struct tsf_stream* stream) { TSFR(modSrcOper) TSFR(modDestOper) TSFR(modAmount) TSFR(modAmtSrcOper) TSFR(modTransOper) }
static void tsf_hydra_read_pgen(struct tsf_hydra_pgen* i, struct tsf_stream* stream) { TSFR(genOper) TSFR(genAmount) }
static void tsf_hydra_read_inst(struct tsf_hydra_inst* i, struct tsf_stream* stream) { TSFR(instName) TSFR(instBagNdx) }
static void tsf_hydra_read_ibag(struct tsf_hydra_ibag* i, struct tsf_stream* stream) { TSFR(instGenNdx) TSFR(instModNdx) }
//static void tsf_hydra_read_imod(struct tsf_hydra_imod* i, struct tsf_stream* stream) { TSFR(modSrcOper) TSFR(modDestOper) TSFR(modAmount) TSFR(modAmtSrcOper) TSFR(modTransOper) }
static void tsf_hydra_read_igen(struct tsf_hydra_igen* i, struct tsf_stream* stream) { TSFR(genOper) TSFR(genAmount) }
static void tsf_hydra_read_shdr(struct tsf_hydra_shdr* i, struct tsf_stream* stream) { TSFR(sampleName) TSFR(start) TSFR(end) TSFR(startLoop) TSFR(endLoop) TSFR(sampleRate) TSFR(originalPitch) TSFR(pitchCorrection) TSFR(sampleLink) TSFR(sampleType) }
#undef TSFR
enum
{
	phdrSizeInFile = 38, pbagSizeInFile =  4, pmodSizeInFile = 10,
	pgenSizeInFile =  4, instSizeInFile = 22, ibagSizeInFile =  4,
	imodSizeInFile = 10, igenSizeInFile =  4, shdrSizeInFile = 46
};

#define TGET(TYPE) \
static struct tsf_hydra_##TYPE *get_##TYPE(struct tsf_hydra *t, int idx, struct tsf_hydra_##TYPE *data) \
{ \
	t->stream->seek(t->stream->data, t->TYPE##Offset + TYPE##SizeInFile * idx); \
	tsf_hydra_read_##TYPE(data, t->stream); \
	return data; \
}

TGET(phdr)
TGET(pbag)
//TGET(pmod)
TGET(pgen)
TGET(inst)
TGET(ibag)
//TGET(imod)
TGET(igen)
TGET(shdr)
#undef TGET

typedef int64_t fixed32p32;
typedef int32_t fixed30p2;
typedef int32_t fixed24p8;
typedef int32_t fixed16p16;
typedef int32_t fixed8p24;



struct tsf_riffchunk { tsf_fourcc id; tsf_u32 size; };
struct tsf_envelope { float delay, start, attack, hold, decay, sustain, release, keynumToHold, keynumToDecay; };
struct tsf_voice_envelope { float level, slope; int samplesUntilNextSegment; int segment; struct tsf_envelope parameters; TSF_BOOL segmentIsExponential, exponentialDecay; };
struct tsf_voice_lowpass { double QInv, a0, a1, b1, b2, z1, z2; TSF_BOOL active; };
struct tsf_voice_lfo { int samplesUntil; float level, delta; };

struct tsf_region
{
	int loop_mode;
	unsigned int sample_rate;
	unsigned char lokey, hikey, lovel, hivel;
	unsigned int group, offset, end, loop_start, loop_end;
	int transpose, tune, pitch_keycenter, pitch_keytrack;
	float volume, pan;
	struct tsf_envelope ampenv, modenv;
	int initialFilterQ, initialFilterFc;
	int modEnvToPitch, modEnvToFilterFc, modLfoToFilterFc, modLfoToVolume;
	float delayModLFO;
	int freqModLFO, modLfoToPitch;
	float delayVibLFO;
	int freqVibLFO, vibLfoToPitch;
};

struct tsf_preset
{
	tsf_char20 presetName;
	tsf_u16 preset, bank;
	struct tsf_region* regions;
	int regionNum;
};

struct tsf_voice
{
	int playingPreset, playingKey, curPitchWheel;
	struct tsf_region* region;
	double pitchInputTimecents, pitchOutputFactor;
	double sourceSamplePosition;
  fixed32p32 sourceSamplePositionF32P32;
	float  noteGainDB, panFactorLeft, panFactorRight;
	unsigned int sampleEnd, loopStart, loopEnd;
	struct tsf_voice_envelope ampenv, modenv;
	struct tsf_voice_lowpass lowpass;
	struct tsf_voice_lfo modlfo, viblfo;
};

static double tsf_timecents2Secsd(double timecents) { return TSF_POW(2.0, timecents / 1200.0); }
static float tsf_timecents2Secsf(float timecents) { return TSF_POWF(2.0f, timecents / 1200.0f); }
static float tsf_cents2Hertz(float cents) { return 8.176f * TSF_POWF(2.0f, cents / 1200.0f); }
static float tsf_decibelsToGain(float db) { return (db > -100.f ? TSF_POWF(10.0f, db * 0.05f) : 0); }

static TSF_BOOL tsf_riffchunk_read(struct tsf_riffchunk* parent, struct tsf_riffchunk* chunk, struct tsf_stream* stream)
{
	TSF_BOOL IsRiff, IsList;
	if (parent && sizeof(tsf_fourcc) + sizeof(tsf_u32) > parent->size) return TSF_FALSE;
	if (!stream->read(stream->data, &chunk->id, sizeof(tsf_fourcc)) || *chunk->id <= ' ' || *chunk->id >= 'z') return TSF_FALSE;
	if (!stream->read(stream->data, &chunk->size, sizeof(tsf_u32))) return TSF_FALSE;
	if (parent && sizeof(tsf_fourcc) + sizeof(tsf_u32) + chunk->size > parent->size) return TSF_FALSE;
	if (parent) parent->size -= sizeof(tsf_fourcc) + sizeof(tsf_u32) + chunk->size;
	IsRiff = TSF_FourCCEquals(chunk->id, "RIFF"), IsList = TSF_FourCCEquals(chunk->id, "LIST");
	if (IsRiff && parent) return TSF_FALSE; //not allowed
	if (!IsRiff && !IsList) return TSF_TRUE; //custom type without sub type
	if (!stream->read(stream->data, &chunk->id, sizeof(tsf_fourcc)) || *chunk->id <= ' ' || *chunk->id >= 'z') return TSF_FALSE;
	chunk->size -= sizeof(tsf_fourcc);
	return TSF_TRUE;
}

static void tsf_region_clear(struct tsf_region* i, TSF_BOOL for_relative)
{
	TSF_MEMSET(i, 0, sizeof(struct tsf_region));
	i->hikey = i->hivel = 127;
	i->pitch_keycenter = 60; // C4
	if (for_relative) return;

	i->pitch_keytrack = 100;

	i->pitch_keycenter = -1;

	// SF2 defaults in timecents.
	i->ampenv.delay = i->ampenv.attack = i->ampenv.hold = i->ampenv.decay = i->ampenv.release = -12000.0f;
	i->modenv.delay = i->modenv.attack = i->modenv.hold = i->modenv.decay = i->modenv.release = -12000.0f;

	i->initialFilterFc = 13500;

	i->delayModLFO = -12000.0f;
	i->delayVibLFO = -12000.0f;
}

static void tsf_region_operator(struct tsf_region* region, tsf_u16 genOper, union tsf_hydra_genamount* amount)
{
	enum
	{
		StartAddrsOffset, EndAddrsOffset, StartloopAddrsOffset, EndloopAddrsOffset, StartAddrsCoarseOffset, ModLfoToPitch, VibLfoToPitch, ModEnvToPitch,
		InitialFilterFc, InitialFilterQ, ModLfoToFilterFc, ModEnvToFilterFc, EndAddrsCoarseOffset, ModLfoToVolume, Unused1, ChorusEffectsSend,
		ReverbEffectsSend, Pan, Unused2, Unused3, Unused4, DelayModLFO, FreqModLFO, DelayVibLFO, FreqVibLFO, DelayModEnv, AttackModEnv, HoldModEnv,
		DecayModEnv, SustainModEnv, ReleaseModEnv, KeynumToModEnvHold, KeynumToModEnvDecay, DelayVolEnv, AttackVolEnv, HoldVolEnv, DecayVolEnv,
		SustainVolEnv, ReleaseVolEnv, KeynumToVolEnvHold, KeynumToVolEnvDecay, Instrument, Reserved1, KeyRange, VelRange, StartloopAddrsCoarseOffset,
		Keynum, Velocity, InitialAttenuation, Reserved2, EndloopAddrsCoarseOffset, CoarseTune, FineTune, SampleID, SampleModes, Reserved3, ScaleTuning,
		ExclusiveClass, OverridingRootKey, Unused5, EndOper
	};
	switch (genOper)
	{
		case StartAddrsOffset:           region->offset += amount->shortAmount; break;
		case EndAddrsOffset:             region->end += amount->shortAmount; break;
		case StartloopAddrsOffset:       region->loop_start += amount->shortAmount; break;
		case EndloopAddrsOffset:         region->loop_end += amount->shortAmount; break;
		case StartAddrsCoarseOffset:     region->offset += amount->shortAmount * 32768; break;
		case ModLfoToPitch:              region->modLfoToPitch = amount->shortAmount; break;
		case VibLfoToPitch:              region->vibLfoToPitch = amount->shortAmount; break;
		case ModEnvToPitch:              region->modEnvToPitch = amount->shortAmount; break;
		case InitialFilterFc:            region->initialFilterFc = amount->shortAmount; break;
		case InitialFilterQ:             region->initialFilterQ = amount->shortAmount; break;
		case ModLfoToFilterFc:           region->modLfoToFilterFc = amount->shortAmount; break;
		case ModEnvToFilterFc:           region->modEnvToFilterFc = amount->shortAmount; break;
		case EndAddrsCoarseOffset:       region->end += amount->shortAmount * 32768; break;
		case ModLfoToVolume:             region->modLfoToVolume = amount->shortAmount; break;
		case Pan:                        region->pan = amount->shortAmount * (2.0f / 10.0f); break;
		case DelayModLFO:                region->delayModLFO = amount->shortAmount; break;
		case FreqModLFO:                 region->freqModLFO = amount->shortAmount; break;
		case DelayVibLFO:                region->delayVibLFO = amount->shortAmount; break;
		case FreqVibLFO:                 region->freqVibLFO = amount->shortAmount; break;
		case DelayModEnv:                region->modenv.delay = amount->shortAmount; break;
		case AttackModEnv:               region->modenv.attack = amount->shortAmount; break;
		case HoldModEnv:                 region->modenv.hold = amount->shortAmount; break;
		case DecayModEnv:                region->modenv.decay = amount->shortAmount; break;
		case SustainModEnv:              region->modenv.sustain = amount->shortAmount; break;
		case ReleaseModEnv:              region->modenv.release = amount->shortAmount; break;
		case KeynumToModEnvHold:         region->modenv.keynumToHold = amount->shortAmount; break;
		case KeynumToModEnvDecay:        region->modenv.keynumToDecay = amount->shortAmount; break;
		case DelayVolEnv:                region->ampenv.delay = amount->shortAmount; break;
		case AttackVolEnv:               region->ampenv.attack = amount->shortAmount; break;
		case HoldVolEnv:                 region->ampenv.hold = amount->shortAmount; break;
		case DecayVolEnv:                region->ampenv.decay = amount->shortAmount; break;
		case SustainVolEnv:              region->ampenv.sustain = amount->shortAmount; break;
		case ReleaseVolEnv:              region->ampenv.release = amount->shortAmount; break;
		case KeynumToVolEnvHold:         region->ampenv.keynumToHold = amount->shortAmount; break;
		case KeynumToVolEnvDecay:        region->ampenv.keynumToDecay = amount->shortAmount; break;
		case KeyRange:                   region->lokey = amount->range.lo; region->hikey = amount->range.hi; break;
		case VelRange:                   region->lovel = amount->range.lo; region->hivel = amount->range.hi; break;
		case StartloopAddrsCoarseOffset: region->loop_start += amount->shortAmount * 32768; break;
		case InitialAttenuation:         region->volume += -amount->shortAmount / 100.0f; break;
		case EndloopAddrsCoarseOffset:   region->loop_end += amount->shortAmount * 32768; break;
		case CoarseTune:                 region->transpose += amount->shortAmount; break;
		case FineTune:                   region->tune += amount->shortAmount; break;
		case SampleModes:                region->loop_mode = ((amount->wordAmount&3) == 3 ? TSF_LOOPMODE_SUSTAIN : ((amount->wordAmount&3) == 1 ? TSF_LOOPMODE_CONTINUOUS : TSF_LOOPMODE_NONE)); break;
		case ScaleTuning:                region->pitch_keytrack = amount->shortAmount; break;
		case ExclusiveClass:             region->group = amount->wordAmount; break;
		case OverridingRootKey:          region->pitch_keycenter = amount->shortAmount; break;
		//case gen_endOper: break; // Ignore.
		//default: addUnsupportedOpcode(generator_name);
	}
}

static void tsf_region_envtosecs(struct tsf_envelope* p, TSF_BOOL sustainIsGain)
{
	// EG times need to be converted from timecents to seconds.
	// Pin very short EG segments.  Timecents don't get to zero, and our EG is
	// happier with zero values.
	p->delay   = (p->delay   < -11950.0f ? 0.0f : tsf_timecents2Secsf(p->delay));
	p->attack  = (p->attack  < -11950.0f ? 0.0f : tsf_timecents2Secsf(p->attack));
	p->release = (p->release < -11950.0f ? 0.0f : tsf_timecents2Secsf(p->release));

	// If we have dynamic hold or decay times depending on key number we need
	// to keep the values in timecents so we can calculate it during startNote
	if (!p->keynumToHold)  p->hold  = (p->hold  < -11950.0f ? 0.0f : tsf_timecents2Secsf(p->hold));
	if (!p->keynumToDecay) p->decay = (p->decay < -11950.0f ? 0.0f : tsf_timecents2Secsf(p->decay));
	
	if (p->sustain < 0.0f) p->sustain = 0.0f;
	else if (sustainIsGain) p->sustain = 100.0f * tsf_decibelsToGain(-p->sustain / 10.0f);
	else p->sustain = p->sustain / 10.0f;
}

static void tsf_load_preset(tsf* res, struct tsf_hydra *hydra, int presetToLoad)
{
	enum { GenInstrument = 41, GenSampleID = 53 };
	// Read each preset.
	struct tsf_hydra_phdr phdr;
	int phdrIdx, phdrMaxIdx;
	for (phdrIdx = presetToLoad, get_phdr(hydra, phdrIdx, &phdr), phdrMaxIdx = presetToLoad + 1 /*hydra->phdrNum - 1*/; phdrIdx != phdrMaxIdx; phdrIdx++, get_phdr(hydra, phdrIdx, &phdr))
	{
		int sortedIndex = 0, region_index = 0;
		struct tsf_hydra_phdr otherPhdr;
		int otherPhdrIdx;
		struct tsf_preset* preset;
		for (otherPhdrIdx = 0, get_phdr(hydra, otherPhdrIdx, &otherPhdr); otherPhdrIdx != phdrMaxIdx; otherPhdrIdx++, get_phdr(hydra, otherPhdrIdx, &otherPhdr))
		{
			if (otherPhdrIdx == phdrIdx || otherPhdr.bank > phdr.bank) continue;
			else if (otherPhdr.bank < phdr.bank) sortedIndex++;
			else if (otherPhdr.preset > phdr.preset) continue;
			else if (otherPhdr.preset < phdr.preset) sortedIndex++;
			else if (otherPhdrIdx < phdrIdx) sortedIndex++;
		}

		preset = &res->presets[sortedIndex];
		TSF_MEMCPY(preset->presetName, phdr.presetName, sizeof(preset->presetName));
		preset->presetName[sizeof(preset->presetName)-1] = '\0'; //should be zero terminated in source file but make sure
		preset->bank = phdr.bank;
		preset->preset = phdr.preset;
		preset->regionNum = 0;

		struct tsf_hydra_phdr phdrNext;
		get_phdr(hydra, phdrIdx + 1, &phdrNext);

		//count regions covered by this preset
		struct tsf_hydra_pbag pbag;
		int pbagIdx, pbagEndIdx;
		for (pbagIdx = phdr.presetBagNdx, get_pbag(hydra, pbagIdx, &pbag), pbagEndIdx = phdrNext.presetBagNdx; pbagIdx != pbagEndIdx; pbagIdx++, get_pbag(hydra, pbagIdx, &pbag))
		{
			struct tsf_hydra_pbag pbagNext;
			get_pbag(hydra, pbagIdx + 1, &pbagNext);
			struct tsf_hydra_pgen pgen;
			int pgenIdx, pgenEndIdx;
			for (pgenIdx = pbag.genNdx, get_pgen(hydra, pgenIdx, &pgen), pgenEndIdx = pbagNext.genNdx; pgenIdx != pgenEndIdx; pgenIdx++, get_pgen(hydra, pgenIdx, &pgen))
			{
				if (pgen.genOper != GenInstrument) continue;
				if (pgen.genAmount.wordAmount >= hydra->instNum) continue;
				struct tsf_hydra_inst inst, instNext;
				get_inst(hydra, pgen.genAmount.wordAmount, &inst);
				get_inst(hydra, pgen.genAmount.wordAmount+1, &instNext);
				struct tsf_hydra_ibag ibag;
				int ibagIdx, ibagEndIdx;
				for (ibagIdx = inst.instBagNdx, get_ibag(hydra, ibagIdx, &ibag), ibagEndIdx = instNext.instBagNdx; ibagIdx != ibagEndIdx; ibagIdx++, get_ibag(hydra, ibagIdx, &ibag) )
				{
					struct tsf_hydra_ibag ibagNext;
					get_ibag(hydra, ibagIdx + 1, &ibagNext);
					struct tsf_hydra_igen igen;
					int igenIdx, igenEndIdx;
					for (igenIdx = ibag.instGenNdx, get_igen(hydra, igenIdx, &igen), igenEndIdx = ibagNext.instGenNdx; igenIdx != igenEndIdx; igenIdx++, get_igen(hydra, igenIdx, &igen))
						if (igen.genOper == GenSampleID)
							preset->regionNum++;
				}
			}
		}

		preset->regions = (struct tsf_region*)TSF_MALLOC(preset->regionNum * sizeof(struct tsf_region));

		// Zones.
		//*** TODO: Handle global zone (modulators only).
		for (pbagIdx = phdr.presetBagNdx, get_pbag(hydra, pbagIdx, &pbag), pbagEndIdx = phdrNext.presetBagNdx; pbagIdx != pbagEndIdx; pbagIdx++, get_pbag(hydra, pbagIdx, &pbag))
		{
			struct tsf_region presetRegion;
			tsf_region_clear(&presetRegion, TSF_TRUE);
			struct tsf_hydra_pbag pbagNext;
			get_pbag(hydra, pbagIdx + 1, &pbagNext);
			struct tsf_hydra_pgen pgen;
			int pgenIdx, pgenEndIdx;

			// Generators.
			for (pgenIdx = pbag.genNdx, get_pgen(hydra, pgenIdx, &pgen), pgenEndIdx = pbagNext.genNdx; pgenIdx != pgenEndIdx; pgenIdx++, get_pgen(hydra, pgenIdx, &pgen))
			{
				// Instrument.
				if (pgen.genOper == GenInstrument)
				{
					struct tsf_region instRegion;
					tsf_u16 whichInst = pgen.genAmount.wordAmount;
					if (whichInst >= hydra->instNum) continue;

					tsf_region_clear(&instRegion, TSF_FALSE);
					// Preset generators are supposed to be "relative" modifications of
					// the instrument settings, but that makes no sense for ranges.
					// For those, we'll have the instrument's generator take
					// precedence, though that may not be correct.
					instRegion.lokey = presetRegion.lokey;
					instRegion.hikey = presetRegion.hikey;
					instRegion.lovel = presetRegion.lovel;
					instRegion.hivel = presetRegion.hivel;

					struct tsf_hydra_inst inst, instNext;
					get_inst(hydra, whichInst, &inst);
					get_inst(hydra, whichInst + 1, &instNext);
					struct tsf_hydra_ibag ibag;
					int ibagIdx, ibagEndIdx;
					for (ibagIdx = inst.instBagNdx, get_ibag(hydra, ibagIdx, &ibag), ibagEndIdx = instNext.instBagNdx; ibagIdx != ibagEndIdx; ibagIdx++, get_ibag(hydra, ibagIdx, &ibag))
					{
						// Generators.
						struct tsf_region zoneRegion = instRegion;
						int hadSampleID = 0;
						struct tsf_hydra_ibag ibagNext;
						get_ibag(hydra, ibagIdx + 1, &ibagNext);
						struct tsf_hydra_igen igen;
						int igenIdx, igenEndIdx;
						for (igenIdx = ibag.instGenNdx, get_igen(hydra, igenIdx, &igen), igenEndIdx = ibagNext.instGenNdx; igenIdx != igenEndIdx; igenIdx++, get_igen(hydra, igenIdx, &igen))
						{
							if (igen.genOper == GenSampleID)
							{
								struct tsf_hydra_shdr shdr;
								get_shdr(hydra, igen.genAmount.wordAmount, &shdr);

								//sum regions
								zoneRegion.offset += presetRegion.offset;
								zoneRegion.end += presetRegion.end;
								zoneRegion.loop_start += presetRegion.loop_start;
								zoneRegion.loop_end += presetRegion.loop_end;
								zoneRegion.transpose += presetRegion.transpose;
								zoneRegion.tune += presetRegion.tune;
								zoneRegion.pitch_keytrack += presetRegion.pitch_keytrack;
								zoneRegion.volume += presetRegion.volume;
								zoneRegion.pan += presetRegion.pan;
								zoneRegion.ampenv.delay += presetRegion.ampenv.delay;
								zoneRegion.ampenv.attack += presetRegion.ampenv.attack;
								zoneRegion.ampenv.hold += presetRegion.ampenv.hold;
								zoneRegion.ampenv.decay += presetRegion.ampenv.decay;
								zoneRegion.ampenv.sustain += presetRegion.ampenv.sustain;
								zoneRegion.ampenv.release += presetRegion.ampenv.release;
								zoneRegion.modenv.delay += presetRegion.modenv.delay;
								zoneRegion.modenv.attack += presetRegion.modenv.attack;
								zoneRegion.modenv.hold += presetRegion.modenv.hold;
								zoneRegion.modenv.decay += presetRegion.modenv.decay;
								zoneRegion.modenv.sustain += presetRegion.modenv.sustain;
								zoneRegion.modenv.release += presetRegion.modenv.release;
								zoneRegion.initialFilterQ += presetRegion.initialFilterQ;
								zoneRegion.initialFilterFc += presetRegion.initialFilterFc;
								zoneRegion.modEnvToPitch += presetRegion.modEnvToPitch;
								zoneRegion.modEnvToFilterFc += presetRegion.modEnvToFilterFc;
								zoneRegion.delayModLFO += presetRegion.delayModLFO;
								zoneRegion.freqModLFO += presetRegion.freqModLFO;
								zoneRegion.modLfoToPitch += presetRegion.modLfoToPitch;
								zoneRegion.modLfoToFilterFc += presetRegion.modLfoToFilterFc;
								zoneRegion.modLfoToVolume += presetRegion.modLfoToVolume;
								zoneRegion.delayVibLFO += presetRegion.delayVibLFO;
								zoneRegion.freqVibLFO += presetRegion.freqVibLFO;
								zoneRegion.vibLfoToPitch += presetRegion.vibLfoToPitch;

								// EG times need to be converted from timecents to seconds.
								tsf_region_envtosecs(&zoneRegion.ampenv, TSF_TRUE);
								tsf_region_envtosecs(&zoneRegion.modenv, TSF_FALSE);

								// LFO times need to be converted from timecents to seconds.
								zoneRegion.delayModLFO = (zoneRegion.delayModLFO < -11950.0f ? 0.0f : tsf_timecents2Secsf(zoneRegion.delayModLFO));
								zoneRegion.delayVibLFO = (zoneRegion.delayVibLFO < -11950.0f ? 0.0f : tsf_timecents2Secsf(zoneRegion.delayVibLFO));

								// Pin values to their ranges.
								if (zoneRegion.pan < -100.0f) zoneRegion.pan = -100.0f;
								else if (zoneRegion.pan > 100.0f) zoneRegion.pan = 100.0f;
								if (zoneRegion.initialFilterQ < 1500 || zoneRegion.initialFilterQ > 13500) zoneRegion.initialFilterQ = 0;

								zoneRegion.offset += shdr.start;
								zoneRegion.end += shdr.end;
								zoneRegion.loop_start += shdr.startLoop;
								zoneRegion.loop_end += shdr.endLoop;
								if (shdr.endLoop > 0) zoneRegion.loop_end -= 1;
								if (zoneRegion.pitch_keycenter == -1) zoneRegion.pitch_keycenter = shdr.originalPitch;
								zoneRegion.tune += shdr.pitchCorrection;

								// Pin initialAttenuation to max +6dB.
								if (zoneRegion.volume > 6.0f)
								{
									zoneRegion.volume = 6.0f;
									//addUnsupportedOpcode("extreme gain in initialAttenuation");
								}

								preset->regions[region_index] = zoneRegion;
								preset->regions[region_index].sample_rate = shdr.sampleRate;
								region_index++;
								hadSampleID = 1;
							}
							else tsf_region_operator(&zoneRegion, igen.genOper, &igen.genAmount);
						}

						// Handle instrument's global zone.
						if (ibagIdx == inst.instBagNdx && !hadSampleID)
							instRegion = zoneRegion;

						// Modulators (TODO)
						//if (ibag->instModNdx < ibag[1].instModNdx) addUnsupportedOpcode("any modulator");
					}
				}
				else tsf_region_operator(&presetRegion, pgen.genOper, &pgen.genAmount);
			}

			// Modulators (TODO)
			//if (pbag->modNdx < pbag[1].modNdx) addUnsupportedOpcode("any modulator");
		}
	}
}

static void tsf_load_samples(int *fontSamplesOffset, int* fontSampleCount, struct tsf_riffchunk *chunkSmpl, struct tsf_stream* stream)
{
	// Read sample data into float format buffer.
	unsigned int samplesLeft;
	samplesLeft = *fontSampleCount = chunkSmpl->size / sizeof(short);
	*fontSamplesOffset = stream->tell(stream->data);
	stream->skip(stream->data, samplesLeft * sizeof(short));
}

static void tsf_voice_envelope_nextsegment(struct tsf_voice_envelope* e, int active_segment, float outSampleRate)
{
	switch (active_segment)
	{
		case TSF_SEGMENT_NONE:
			e->samplesUntilNextSegment = (int)(e->parameters.delay * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_DELAY;
				e->segmentIsExponential = TSF_FALSE;
				e->level = 0.0;
				e->slope = 0.0;
				return;
			}
		case TSF_SEGMENT_DELAY:
			e->samplesUntilNextSegment = (int)(e->parameters.attack * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_ATTACK;
				e->segmentIsExponential = TSF_FALSE;
				e->level = e->parameters.start / 100.0f;
				e->slope = 1.0f / e->samplesUntilNextSegment;
				return;
			}
		case TSF_SEGMENT_ATTACK:
			e->samplesUntilNextSegment = (int)(e->parameters.hold * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_HOLD;
				e->segmentIsExponential = TSF_FALSE;
				e->level = 1.0;
				e->slope = 0.0;
				return;
			}
		case TSF_SEGMENT_HOLD:
			e->samplesUntilNextSegment = (int)(e->parameters.decay * outSampleRate);
			if (e->samplesUntilNextSegment > 0)
			{
				e->segment = TSF_SEGMENT_DECAY;
				e->level = 1.0;
				if (e->exponentialDecay)
				{
					// I don't truly understand this; just following what LinuxSampler does.
					float mysterySlope = -9.226f / e->samplesUntilNextSegment;
					e->slope = TSF_EXPF(mysterySlope);
					e->segmentIsExponential = TSF_TRUE;
					if (e->parameters.sustain > 0.0f)
					{
						// Again, this is following LinuxSampler's example, which is similar to
						// SF2-style decay, where "decay" specifies the time it would take to
						// get to zero, not to the sustain level.  The SFZ spec is not that
						// specific about what "decay" means, so perhaps it's really supposed
						// to specify the time to reach the sustain level.
						e->samplesUntilNextSegment = (int)(TSF_LOG((e->parameters.sustain / 100.0) / e->level) / mysterySlope);
					}
				}
				else
				{
					e->slope = (e->parameters.sustain / 100.0f - 1.0f) / e->samplesUntilNextSegment;
					e->segmentIsExponential = TSF_FALSE;
				}
				return;
			}
		case TSF_SEGMENT_DECAY:
			e->segment = TSF_SEGMENT_SUSTAIN;
			e->level = e->parameters.sustain / 100.0f;
			e->slope = 0.0f;
			e->samplesUntilNextSegment = 0x7FFFFFFF;
			e->segmentIsExponential = TSF_FALSE;
			return;
		case TSF_SEGMENT_SUSTAIN:
			e->segment = TSF_SEGMENT_RELEASE;
			e->samplesUntilNextSegment = (int)((e->parameters.release <= 0 ? TSF_FASTRELEASETIME : e->parameters.release) * outSampleRate);
			if (e->exponentialDecay)
			{
				// I don't truly understand this; just following what LinuxSampler does.
				float mysterySlope = -9.226f / e->samplesUntilNextSegment;
				e->slope = TSF_EXPF(mysterySlope);
				e->segmentIsExponential = TSF_TRUE;
			}
			else
			{
				e->slope = -e->level / e->samplesUntilNextSegment;
				e->segmentIsExponential = TSF_FALSE;
			}
			return;
		case TSF_SEGMENT_RELEASE:
		default:
			e->segment = TSF_SEGMENT_DONE;
			e->segmentIsExponential = TSF_FALSE;
			e->level = e->slope = 0;
			e->samplesUntilNextSegment = 0x7FFFFFF;
	}
}

static void tsf_voice_envelope_setup(struct tsf_voice_envelope* e, struct tsf_envelope* new_parameters, int midiNoteNumber, TSF_BOOL setExponentialDecay, float outSampleRate)
{
	e->parameters = *new_parameters;
	if (e->parameters.keynumToHold)
	{
		e->parameters.hold += e->parameters.keynumToHold * (60.0f - midiNoteNumber);
		e->parameters.hold = (e->parameters.hold < -10000.0f ? 0.0f : tsf_timecents2Secsf(e->parameters.hold));
	}
	if (e->parameters.keynumToDecay)
	{
		e->parameters.decay += e->parameters.keynumToDecay * (60.0f - midiNoteNumber);
		e->parameters.decay = (e->parameters.decay < -10000.0f ? 0.0f : tsf_timecents2Secsf(e->parameters.decay));
	}
	e->exponentialDecay = setExponentialDecay; 
	tsf_voice_envelope_nextsegment(e, TSF_SEGMENT_NONE, outSampleRate);
}

static void tsf_voice_envelope_process(struct tsf_voice_envelope* e, int numSamples, float outSampleRate)
{
	if (e->slope)
	{
		if (e->segmentIsExponential) e->level *= TSF_POWF(e->slope, (float)numSamples);
		else e->level += (e->slope * numSamples);
	}
	if ((e->samplesUntilNextSegment -= numSamples) <= 0)
		tsf_voice_envelope_nextsegment(e, e->segment, outSampleRate);
}

static void tsf_voice_lowpass_setup(struct tsf_voice_lowpass* e, float Fc)
{
	// Lowpass filter from http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
	double K = TSF_TAN(TSF_PI * Fc), KK = K * K;
	double norm = 1 / (1 + K * e->QInv + KK);
	e->a0 = KK * norm;
	e->a1 = 2 * e->a0;
	e->b1 = 2 * (KK - 1) * norm;
	e->b2 = (1 - K * e->QInv + KK) * norm;
}

static float tsf_voice_lowpass_process(struct tsf_voice_lowpass* e, double In)
{
	double Out = In * e->a0 + e->z1; e->z1 = In * e->a1 + e->z2 - e->b1 * Out; e->z2 = In * e->a0 - e->b2 * Out; return (float)Out;
}

static void tsf_voice_lfo_setup(struct tsf_voice_lfo* e, float delay, int freqCents, float outSampleRate)
{
	e->samplesUntil = (int)(delay * outSampleRate);
	e->delta = (4.0f * tsf_cents2Hertz((float)freqCents) / outSampleRate);
	e->level = 0;
}

static void tsf_voice_lfo_process(struct tsf_voice_lfo* e, int blockSamples)
{
	if (e->samplesUntil > blockSamples) { e->samplesUntil -= blockSamples; return; }
	e->level += e->delta * blockSamples;
	if      (e->level >  1.0f) { e->delta = -e->delta; e->level =  2.0f - e->level; }
	else if (e->level < -1.0f) { e->delta = -e->delta; e->level = -2.0f - e->level; }
}

static void tsf_voice_kill(struct tsf_voice* v)
{
	v->region = TSF_NULL;
	v->playingPreset = -1;
}

static void tsf_voice_end(struct tsf_voice* v, float outSampleRate)
{
	tsf_voice_envelope_nextsegment(&v->ampenv, TSF_SEGMENT_SUSTAIN, outSampleRate);
	tsf_voice_envelope_nextsegment(&v->modenv, TSF_SEGMENT_SUSTAIN, outSampleRate);
	if (v->region->loop_mode == TSF_LOOPMODE_SUSTAIN)
	{
		// Continue playing, but stop looping.
		v->loopEnd = v->loopStart;
	}
}

static void tsf_voice_endquick(struct tsf_voice* v, float outSampleRate)
{
	v->ampenv.parameters.release = 0.0f; tsf_voice_envelope_nextsegment(&v->ampenv, TSF_SEGMENT_SUSTAIN, outSampleRate);
	v->modenv.parameters.release = 0.0f; tsf_voice_envelope_nextsegment(&v->modenv, TSF_SEGMENT_SUSTAIN, outSampleRate);
}

static void tsf_voice_calcpitchratio(struct tsf_voice* v, float outSampleRate)
{
	double note = v->playingKey, adjustedPitch;
	note += v->region->transpose;
	note += v->region->tune / 100.0;

	adjustedPitch = v->region->pitch_keycenter + (note - v->region->pitch_keycenter) * (v->region->pitch_keytrack / 100.0);
	if (v->curPitchWheel != 8192) adjustedPitch += ((4.0 * v->curPitchWheel / 16383.0) - 2.0);

	v->pitchInputTimecents = adjustedPitch * 100.0;
	v->pitchOutputFactor = v->region->sample_rate / (tsf_timecents2Secsd(v->region->pitch_keycenter * 100.0) * outSampleRate);
}

short tsf_read_short_cached(tsf *f, int pos)
{
	static int hits = 0;
	static int misses = 0;
//	static int call =0;
//	call++;
//	if ((call % 88000) ==0) printf("Hit: %d, Miss: %d, Ratio: %f\n", hits, misses, (double)hits/(double)(misses+hits));

	for (int i=0; i<TSF_BUFFS; i++) {
		if ((f->offset[i] <= pos) && ((f->offset[i] + TSF_BUFFSIZE) > pos) ) {
			f->timestamp[i] = f->epoch++;
			if (f->epoch==0) {
				for (int i=0; i<TSF_BUFFS; i++) f->timestamp[i] = f->epoch++;
			}
			hits++;
      return f->buffer[i][pos - f->offset[i]];
		}
	}
	int repl = 0;
	for (int i=1; i<TSF_BUFFS; i++) {
		if (f->timestamp[i] < f->timestamp[repl]) repl = i;
	}
	int readOff = pos - (pos % TSF_BUFFSIZE);
// for (int i=0; i<TSF_BUFFSIZE; i++) { f->buffer[repl][i] = i; }
	f->hydra->stream->seek(f->hydra->stream->data, readOff * sizeof(short));
	f->hydra->stream->read(f->hydra->stream->data, f->buffer[repl], TSF_BUFFSIZE * sizeof(short));
//static uint32_t *bp = NULL; if (!bp) bp = (uint32_t*)malloc(512);
//printf("off=%08x, buff=%p, len=%08x\n", readOff * sizeof(short), bp, TSF_BUFFSIZE); spi_flash_read(0x0000, bp, 512/4-4) ;
	f->timestamp[repl] = f->epoch++;
	f->offset[repl] = readOff;
	misses++;
	return f->buffer[repl][pos - readOff];
}

static void tsf_voice_render(tsf* f, struct tsf_voice* v, float* outputBuffer, int numSamples)
{
	struct tsf_region* region = v->region;
	float* outL = outputBuffer;
	float* outR = (f->outputmode == TSF_STEREO_UNWEAVED ? outL + numSamples : TSF_NULL);

	// Cache some values, to give them at least some chance of ending up in registers.
	TSF_BOOL updateModEnv = (region->modEnvToPitch || region->modEnvToFilterFc);
	TSF_BOOL updateModLFO = (v->modlfo.delta && (region->modLfoToPitch || region->modLfoToFilterFc || region->modLfoToVolume));
	TSF_BOOL updateVibLFO = (v->viblfo.delta && (region->vibLfoToPitch));
	TSF_BOOL isLooping    = (v->loopStart < v->loopEnd);
	unsigned int tmpLoopStart = v->loopStart, tmpLoopEnd = v->loopEnd;
	double tmpSampleEndDbl = (double)v->sampleEnd, tmpLoopEndDbl = (double)tmpLoopEnd + 1.0;
	double tmpSourceSamplePosition = v->sourceSamplePosition;
	struct tsf_voice_lowpass tmpLowpass = v->lowpass;

	TSF_BOOL dynamicLowpass = (region->modLfoToFilterFc || region->modEnvToFilterFc);
	float tmpSampleRate, tmpInitialFilterFc, tmpModLfoToFilterFc, tmpModEnvToFilterFc;

	TSF_BOOL dynamicPitchRatio = (region->modLfoToPitch || region->modEnvToPitch || region->vibLfoToPitch);
	double pitchRatio;
	float tmpModLfoToPitch, tmpVibLfoToPitch, tmpModEnvToPitch;

	TSF_BOOL dynamicGain = (region->modLfoToVolume != 0);
	float noteGain, tmpModLfoToVolume;

	if (dynamicLowpass) tmpSampleRate = f->outSampleRate, tmpInitialFilterFc = (float)region->initialFilterFc, tmpModLfoToFilterFc = (float)region->modLfoToFilterFc, tmpModEnvToFilterFc = (float)region->modEnvToFilterFc;
	else tmpSampleRate = 0, tmpInitialFilterFc = 0, tmpModLfoToFilterFc = 0, tmpModEnvToFilterFc = 0;

	if (dynamicPitchRatio) pitchRatio = 0, tmpModLfoToPitch = (float)region->modLfoToPitch, tmpVibLfoToPitch = (float)region->vibLfoToPitch, tmpModEnvToPitch = (float)region->modEnvToPitch;
	else pitchRatio = tsf_timecents2Secsd(v->pitchInputTimecents) * v->pitchOutputFactor, tmpModLfoToPitch = 0, tmpVibLfoToPitch = 0, tmpModEnvToPitch = 0;

	if (dynamicGain) noteGain = 0, tmpModLfoToVolume = (float)region->modLfoToVolume * 0.1f;
	else noteGain = tsf_decibelsToGain(v->noteGainDB), tmpModLfoToVolume = 0;

	while (numSamples)
	{
		float gainMono, gainLeft, gainRight;
		int blockSamples = (numSamples > TSF_RENDER_EFFECTSAMPLEBLOCK ? TSF_RENDER_EFFECTSAMPLEBLOCK : numSamples);
		numSamples -= blockSamples;

		if (dynamicLowpass)
		{
			float fres = tmpInitialFilterFc + v->modlfo.level * tmpModLfoToFilterFc + v->modenv.level * tmpModEnvToFilterFc;
			tmpLowpass.active = (fres <= 13500.0f);
			if (tmpLowpass.active) tsf_voice_lowpass_setup(&tmpLowpass, tsf_cents2Hertz(fres) / tmpSampleRate);
		}

		if (dynamicPitchRatio)
			pitchRatio = tsf_timecents2Secsd(v->pitchInputTimecents + (v->modlfo.level * tmpModLfoToPitch + v->viblfo.level * tmpVibLfoToPitch + v->modenv.level * tmpModEnvToPitch)) * v->pitchOutputFactor;

		if (dynamicGain)
			noteGain = tsf_decibelsToGain(v->noteGainDB + (v->modlfo.level * tmpModLfoToVolume));

		gainMono = noteGain * v->ampenv.level;

		// Update EG.
		tsf_voice_envelope_process(&v->ampenv, blockSamples, f->outSampleRate);
		if (updateModEnv) tsf_voice_envelope_process(&v->modenv, blockSamples, f->outSampleRate);

		// Update LFOs.
		if (updateModLFO) tsf_voice_lfo_process(&v->modlfo, blockSamples);
		if (updateVibLFO) tsf_voice_lfo_process(&v->viblfo, blockSamples);

		switch (f->outputmode)
		{
			case TSF_STEREO_INTERLEAVED:
				gainLeft = gainMono * v->panFactorLeft, gainRight = gainMono * v->panFactorRight;
				while (blockSamples-- && tmpSourceSamplePosition < tmpSampleEndDbl)
				{
					unsigned int pos = (unsigned int)tmpSourceSamplePosition, nextPos = (pos >= tmpLoopEnd && isLooping ? tmpLoopStart : pos + 1);
					float inputPos, inputNextPos;
					inputPos = (float)(tsf_read_short_cached(f, pos) / 32767.0);
					inputNextPos = (float)(tsf_read_short_cached(f, nextPos) / 32767.0);
					// Simple linear interpolation.
					float alpha = (float)(tmpSourceSamplePosition - pos), val = (inputPos * (1.0f - alpha) + inputNextPos * alpha);

					// Low-pass filter.
					if (tmpLowpass.active) val = tsf_voice_lowpass_process(&tmpLowpass, val);

					*outL++ += val * gainLeft;
					*outL++ += val * gainRight;

					// Next sample.
					tmpSourceSamplePosition += pitchRatio;
					if (tmpSourceSamplePosition >= tmpLoopEndDbl && isLooping) tmpSourceSamplePosition -= (tmpLoopEnd - tmpLoopStart + 1.0);
				}
				break;

			case TSF_STEREO_UNWEAVED:
				gainLeft = gainMono * v->panFactorLeft, gainRight = gainMono * v->panFactorRight;
				while (blockSamples-- && tmpSourceSamplePosition < tmpSampleEndDbl)
				{
					unsigned int pos = (unsigned int)tmpSourceSamplePosition, nextPos = (pos >= tmpLoopEnd && isLooping ? tmpLoopStart : pos + 1);
					float inputPos, inputNextPos;
					inputPos = (float)(tsf_read_short_cached(f, pos) / 32767.0);
					inputNextPos = (float)(tsf_read_short_cached(f, nextPos) / 32767.0);

					// Simple linear interpolation.
					float alpha = (float)(tmpSourceSamplePosition - pos), val = (inputPos * (1.0f - alpha) + inputNextPos * alpha);

					// Low-pass filter.
					if (tmpLowpass.active) val = tsf_voice_lowpass_process(&tmpLowpass, val);

					*outL++ += val * gainLeft;
					*outR++ += val * gainRight;

					// Next sample.
					tmpSourceSamplePosition += pitchRatio;
					if (tmpSourceSamplePosition >= tmpLoopEndDbl && isLooping) tmpSourceSamplePosition -= (tmpLoopEnd - tmpLoopStart + 1.0);
				}
				break;

			case TSF_MONO:
				while (blockSamples-- && tmpSourceSamplePosition < tmpSampleEndDbl)
				{
					unsigned int pos = (unsigned int)tmpSourceSamplePosition, nextPos = (pos >= tmpLoopEnd && isLooping ? tmpLoopStart : pos + 1);
					float inputPos, inputNextPos;
					inputPos = (float)(tsf_read_short_cached(f, pos) / 32767.0);
					inputNextPos = (float)(tsf_read_short_cached(f, nextPos) / 32767.0);

					// Simple linear interpolation.
					float alpha = (float)(tmpSourceSamplePosition - pos), val = (inputPos * (1.0f - alpha) + inputNextPos * alpha);

					// Low-pass filter.
					if (tmpLowpass.active) val = tsf_voice_lowpass_process(&tmpLowpass, val);

					*outL++ += val * gainMono;

					// Next sample.
					tmpSourceSamplePosition += pitchRatio;
					if (tmpSourceSamplePosition >= tmpLoopEndDbl && isLooping) tmpSourceSamplePosition -= (tmpLoopEnd - tmpLoopStart + 1.0);
				}
				break;
		}

		if (tmpSourceSamplePosition >= tmpSampleEndDbl || v->ampenv.segment == TSF_SEGMENT_DONE)
		{
			tsf_voice_kill(v);
			return;
		}
	}

	v->sourceSamplePosition = tmpSourceSamplePosition;
	if (tmpLowpass.active || dynamicLowpass) v->lowpass = tmpLowpass;
}


void DumpF32P32(char *name, long long x) {
  printf("%s = %08x.%08x\n", name, (int32_t)((x>>32)&0xffffffff), (int32_t)(x&0xffffffff));
}
static void tsf_voice_render_fast(tsf* f, struct tsf_voice* v, short* outputBuffer, int numSamples)
{
  struct tsf_region* region = v->region;
  short* outL = outputBuffer;
  short* outR = (f->outputmode == TSF_STEREO_UNWEAVED ? outL + numSamples : TSF_NULL);

  // Cache some values, to give them at least some chance of ending up in registers.
  TSF_BOOL updateModEnv = (region->modEnvToPitch || region->modEnvToFilterFc);
  TSF_BOOL updateModLFO = (v->modlfo.delta && (region->modLfoToPitch || region->modLfoToFilterFc || region->modLfoToVolume));
  TSF_BOOL updateVibLFO = (v->viblfo.delta && (region->vibLfoToPitch));
  TSF_BOOL isLooping    = (v->loopStart < v->loopEnd);
  unsigned int tmpLoopStart = v->loopStart, tmpLoopEnd = v->loopEnd;
  //double tmpSampleEndDbl = (double)v->sampleEnd, tmpLoopEndDbl = (double)tmpLoopEnd + 1.0;
  //double tmpSourceSamplePosition = v->sourceSamplePosition;
  fixed32p32 tmpSampleEndF32P32 = ((fixed32p32)(v->sampleEnd)) << 32;
  fixed32p32 tmpLoopEndF32P32 = ((fixed32p32)(tmpLoopEnd + 1)) << 32;
  fixed32p32 tmpSourceSamplePositionF32P32 = v->sourceSamplePositionF32P32;
  struct tsf_voice_lowpass tmpLowpass = v->lowpass;

  TSF_BOOL dynamicLowpass = (region->modLfoToFilterFc || region->modEnvToFilterFc);
  float tmpSampleRate, tmpInitialFilterFc, tmpModLfoToFilterFc, tmpModEnvToFilterFc;

  TSF_BOOL dynamicPitchRatio = (region->modLfoToPitch || region->modEnvToPitch || region->vibLfoToPitch);
  //double pitchRatio;
  fixed32p32 pitchRatioF32P32;
  float tmpModLfoToPitch, tmpVibLfoToPitch, tmpModEnvToPitch;

  TSF_BOOL dynamicGain = (region->modLfoToVolume != 0);
  float noteGain, tmpModLfoToVolume;

  if (dynamicLowpass) tmpSampleRate = f->outSampleRate, tmpInitialFilterFc = (float)region->initialFilterFc, tmpModLfoToFilterFc = (float)region->modLfoToFilterFc, tmpModEnvToFilterFc = (float)region->modEnvToFilterFc;
  else tmpSampleRate = 0, tmpInitialFilterFc = 0, tmpModLfoToFilterFc = 0, tmpModEnvToFilterFc = 0;

  if (dynamicPitchRatio) pitchRatioF32P32 = 0, tmpModLfoToPitch = (float)region->modLfoToPitch, tmpVibLfoToPitch = (float)region->vibLfoToPitch, tmpModEnvToPitch = (float)region->modEnvToPitch;
  else {
    double pr = tsf_timecents2Secsd(v->pitchInputTimecents) * v->pitchOutputFactor;
    fixed32p32 adj = 1LL<<32;
    pr *= adj;
    pitchRatioF32P32 = (int64_t)pr, tmpModLfoToPitch = 0, tmpVibLfoToPitch = 0, tmpModEnvToPitch = 0;
  }

  if (dynamicGain) noteGain = 0, tmpModLfoToVolume = (float)region->modLfoToVolume * 0.1f;
  else noteGain = tsf_decibelsToGain(v->noteGainDB), tmpModLfoToVolume = 0;

  while (numSamples)
  {
    float gainMono;
    int blockSamples = (numSamples > TSF_RENDER_EFFECTSAMPLEBLOCK ? TSF_RENDER_EFFECTSAMPLEBLOCK : numSamples);
    numSamples -= blockSamples;

    if (dynamicLowpass)
    {
      float fres = tmpInitialFilterFc + v->modlfo.level * tmpModLfoToFilterFc + v->modenv.level * tmpModEnvToFilterFc;
      tmpLowpass.active = (fres <= 13500.0f);
      if (tmpLowpass.active) tsf_voice_lowpass_setup(&tmpLowpass, tsf_cents2Hertz(fres) / tmpSampleRate);
    }

    if (dynamicPitchRatio) {
      pitchRatioF32P32 = tsf_timecents2Secsd(v->pitchInputTimecents + (v->modlfo.level * tmpModLfoToPitch + v->viblfo.level * tmpVibLfoToPitch + v->modenv.level * tmpModEnvToPitch)) * v->pitchOutputFactor * (1LL<<32);
   }

    if (dynamicGain)
      noteGain = tsf_decibelsToGain(v->noteGainDB + (v->modlfo.level * tmpModLfoToVolume));

    gainMono = noteGain * v->ampenv.level;
    short gainMonoFP = gainMono * 32767;
    
    // Update EG.
    tsf_voice_envelope_process(&v->ampenv, blockSamples, f->outSampleRate);
    if (updateModEnv) tsf_voice_envelope_process(&v->modenv, blockSamples, f->outSampleRate);

    // Update LFOs.
    if (updateModLFO) tsf_voice_lfo_process(&v->modlfo, blockSamples);
    if (updateVibLFO) tsf_voice_lfo_process(&v->viblfo, blockSamples);

    while (blockSamples-- && tmpSourceSamplePositionF32P32 < tmpSampleEndF32P32)
    {
      unsigned int pos = (unsigned int)(tmpSourceSamplePositionF32P32>>32);
      short val = tsf_read_short_cached(f, pos);
      int32_t val32 = (int)val * (int)gainMonoFP;

      *outL++ += val32>>16;
      if (f->outputmode != TSF_MONO) *outR++ += val32>>16;

      // Next sample.
      tmpSourceSamplePositionF32P32 += pitchRatioF32P32;
      if (tmpSourceSamplePositionF32P32 >= tmpLoopEndF32P32 && isLooping)
        tmpSourceSamplePositionF32P32 -= (tmpLoopEndF32P32 - tmpLoopStart + (1LL<<32));
    }

    if (tmpSourceSamplePositionF32P32 >= tmpSampleEndF32P32 || v->ampenv.segment == TSF_SEGMENT_DONE)
    {
      tsf_voice_kill(v);
      return;
    }
  }

  v->sourceSamplePositionF32P32 = tmpSourceSamplePositionF32P32;
  if (tmpLowpass.active || dynamicLowpass) v->lowpass = tmpLowpass;
}




TSFDEF tsf* tsf_load(struct tsf_stream* stream)
{
	tsf* res = TSF_NULL;
	struct tsf_riffchunk chunkHead;
	struct tsf_riffchunk chunkList;
	struct tsf_hydra hydra;
	int fontSamplesOffset = 0;
	int fontSampleCount = 0;

	if (!tsf_riffchunk_read(TSF_NULL, &chunkHead, stream) || !TSF_FourCCEquals(chunkHead.id, "sfbk"))
	{
		//if (e) *e = TSF_INVALID_NOSF2HEADER;
		return res;
	}

	// Read hydra and locate sample data.
	TSF_MEMSET(&hydra, 0, sizeof(hydra));
	hydra.stream = stream;
	while (tsf_riffchunk_read(&chunkHead, &chunkList, stream))
	{
		struct tsf_riffchunk chunk;
		if (TSF_FourCCEquals(chunkList.id, "pdta"))
		{
			while (tsf_riffchunk_read(&chunkList, &chunk, stream))
			{
				#define GetChunkOffset(chunkName) (TSF_FourCCEquals(chunk.id, #chunkName) && !(chunk.size % chunkName##SizeInFile)) \
					{ \
						int num = chunk.size / chunkName##SizeInFile; \
						hydra.chunkName##Num = num; \
						hydra.chunkName##Offset = stream->tell(stream->data); \
						stream->skip(stream->data, num * chunkName##SizeInFile); \
					}
				if      GetChunkOffset(phdr)
				else if GetChunkOffset(pbag)
				else if GetChunkOffset(pmod)
				else if GetChunkOffset(pgen)
				else if GetChunkOffset(inst)
				else if GetChunkOffset(ibag)
				else if GetChunkOffset(imod)
				else if GetChunkOffset(igen)
				else if GetChunkOffset(shdr)
				else stream->skip(stream->data, chunk.size);
				#undef HandleChunk
			}
		}
		else if (TSF_FourCCEquals(chunkList.id, "sdta"))
		{
			while (tsf_riffchunk_read(&chunkList, &chunk, stream))
			{
				if (TSF_FourCCEquals(chunk.id, "smpl"))
				{
					tsf_load_samples(&fontSamplesOffset, &fontSampleCount, &chunk, stream);
				}
				else stream->skip(stream->data, chunk.size);
			}
		}
		else stream->skip(stream->data, chunkList.size);
	}
	if (!hydra.phdrNum || !hydra.pbagNum || !hydra.pmodNum || !hydra.pgenNum || !hydra.instNum || !hydra.ibagNum || !hydra.imodNum || !hydra.igenNum || !hydra.shdrNum)
	{
		//if (e) *e = TSF_INVALID_INCOMPLETE;
	}
	else if (fontSamplesOffset == 0)
	{
		//if (e) *e = TSF_INVALID_NOSAMPLEDATA;
	}
	else
	{
		res = (tsf*)TSF_MALLOC(sizeof(tsf));
		TSF_MEMSET(res, 0, sizeof(tsf));
		res->presetNum = hydra.phdrNum - 1;
		res->presets = (struct tsf_preset*)TSF_MALLOC(res->presetNum * sizeof(struct tsf_preset));
		TSF_MEMSET(res->presets, 0, res->presetNum * sizeof(struct tsf_preset));
		res->fontSamplesOffset = fontSamplesOffset;
		res->fontSampleCount = fontSampleCount;
		res->outSampleRate = 44100.0f;
		res->hydra = (struct tsf_hydra*)TSF_MALLOC(sizeof(struct tsf_hydra));
		TSF_MEMCPY(res->hydra, &hydra, sizeof(*res->hydra));
		res->hydra->stream = (struct tsf_stream*)TSF_MALLOC(sizeof(struct tsf_stream));
		TSF_MEMCPY(res->hydra->stream, stream, sizeof(*res->hydra->stream));

		// Cached sample
		for (int i=0; i<TSF_BUFFS; i++) {
			res->buffer[i] = (short*)TSF_MALLOC(TSF_BUFFSIZE * sizeof(short));
			res->offset[i] = 0xfffffff;
			res->timestamp[i] = -1;
		}
		res->epoch = 0;
	}
	return res;
}

TSFDEF void tsf_close(tsf* f)
{
	struct tsf_preset *preset, *presetEnd;
	if (!f) return;
	for (preset = f->presets, presetEnd = preset + f->presetNum; preset != presetEnd; preset++)
		TSF_FREE(preset->regions);
	TSF_FREE(f->presets);
	TSF_FREE(f->voices);
	TSF_FREE(f->outputSamples);
	f->hydra->stream->close(f->hydra->stream->data);
	TSF_FREE(f->hydra->stream);
	TSF_FREE(f->hydra);
	for (int i=0; i<TSF_BUFFS; i++) TSF_FREE(f->buffer[i]);
	TSF_FREE(f);
}

TSFDEF int tsf_get_presetcount(tsf* f)
{
	return f->presetNum;
}

TSFDEF const char* tsf_get_presetname(tsf* f, int preset)
{
	if (f->presets[preset].regions == NULL && preset >= 0 && preset < f->presetNum) tsf_load_preset(f, f->hydra, preset);
	return (preset < 0 || preset >= f->presetNum ? TSF_NULL : f->presets[preset].presetName);
}

TSFDEF void tsf_set_output(tsf* f, enum TSFOutputMode outputmode, int samplerate, float globalgaindb)
{
	f->outSampleRate = (float)(samplerate >= 1 ? samplerate : 44100.0f);
	f->outputmode = outputmode;
	f->globalGainDB = globalgaindb;
}

TSFDEF void tsf_note_on(tsf* f, int preset, int key, float vel)
{
	int midiVelocity = (int)(vel * 127);
	TSF_BOOL haveGroupedNotesPlaying = TSF_FALSE;
	struct tsf_voice *v, *vEnd; struct tsf_region *region, *regionEnd;

	if (preset < 0 || preset >= f->presetNum) return;
	if (f->presets[preset].regions == NULL) tsf_load_preset(f, f->hydra, preset);

	// Are any grouped notes playing? (Needed for group stopping) Also stop any voices still playing this note.
	for (v = f->voices, vEnd = v + f->voiceNum; v != vEnd; v++)
	{
		if (v->playingPreset != preset) continue;
		if (v->playingKey == key) tsf_voice_endquick(v, f->outSampleRate);
		if (v->region->group) haveGroupedNotesPlaying = TSF_TRUE;
	}

	// Play all matching regions.
	for (region = f->presets[preset].regions, regionEnd = region + f->presets[preset].regionNum; region != regionEnd; region++)
	{
		struct tsf_voice* voice = TSF_NULL; double adjustedPan; TSF_BOOL doLoop; float filterQDB;
		if (key < region->lokey || key > region->hikey || midiVelocity < region->lovel || midiVelocity > region->hivel) continue;

		if (haveGroupedNotesPlaying && region->group)
			for (v = f->voices, vEnd = v + f->voiceNum; v != vEnd; v++)
				if (v->playingPreset == preset && v->region->group == region->group)
					tsf_voice_endquick(v, f->outSampleRate);

		for (v = f->voices, vEnd = v + f->voiceNum; v != vEnd; v++) if (v->playingPreset == -1) { voice = v; break; }
		if (!voice)
		{
			f->voiceNum += 4;
			struct tsf_voice *saveVoice = f->voices;
			f->voices = (struct tsf_voice*)TSF_REALLOC(f->voices, f->voiceNum * sizeof(struct tsf_voice));
			if (!f->voices) {
				f->voices = saveVoice;
				printf("OOM, no room for new voice.  Ignoring note_on\n");
				return;
			}
			voice = &f->voices[f->voiceNum - 4];
			voice[1].playingPreset = voice[2].playingPreset = voice[3].playingPreset = -1;
		}

		voice->region = region;
		voice->playingPreset = preset;
		voice->playingKey = key;

		// Pitch.
		voice->curPitchWheel = 8192;
		tsf_voice_calcpitchratio(voice, f->outSampleRate);

		// Gain.
		voice->noteGainDB = f->globalGainDB + region->volume;
		// Thanks to <http:://www.drealm.info/sfz/plj-sfz.xhtml> for explaining the velocity curve in a way that I could understand, although they mean "log10" when they say "log".
		voice->noteGainDB += (float)(-20.0 * TSF_LOG10(1.0 / vel));
		// The SFZ spec is silent about the pan curve, but a 3dB pan law seems common. This sqrt() curve matches what Dimension LE does; Alchemy Free seems closer to sin(adjustedPan * pi/2).
		adjustedPan = (region->pan + 100.0) / 200.0;
		voice->panFactorLeft = (float)TSF_SQRT(1.0 - adjustedPan);
		voice->panFactorRight = (float)TSF_SQRT(adjustedPan);

		// Offset/end.
		voice->sourceSamplePosition = region->offset;
    		voice->sourceSamplePositionF32P32 = ((int64_t)region->offset)<< 32;
		voice->sampleEnd = f->fontSampleCount;
		if (region->end > 0 && region->end < voice->sampleEnd) voice->sampleEnd = region->end + 1;

		// Loop.
		doLoop = (region->loop_mode != TSF_LOOPMODE_NONE && region->loop_start < region->loop_end);
		voice->loopStart = (doLoop ? region->loop_start : 0);
		voice->loopEnd = (doLoop ? region->loop_end : 0);

		// Setup envelopes.
		tsf_voice_envelope_setup(&voice->ampenv, &region->ampenv, key, TSF_TRUE, f->outSampleRate);
		tsf_voice_envelope_setup(&voice->modenv, &region->modenv, key, TSF_FALSE, f->outSampleRate);

		// Setup lowpass filter.
		filterQDB = region->initialFilterQ / 10.0f;
		voice->lowpass.QInv = 1.0 / TSF_POW(10.0, (filterQDB / 20.0));
		voice->lowpass.z1 = voice->lowpass.z2 = 0;
		voice->lowpass.active = (region->initialFilterFc <= 13500);
		if (voice->lowpass.active) tsf_voice_lowpass_setup(&voice->lowpass, tsf_cents2Hertz((float)region->initialFilterFc) / f->outSampleRate);

		// Setup LFO filters.
		tsf_voice_lfo_setup(&voice->modlfo, region->delayModLFO, region->freqModLFO, f->outSampleRate);
		tsf_voice_lfo_setup(&voice->viblfo, region->delayVibLFO, region->freqVibLFO, f->outSampleRate);
	}
}

TSFDEF void tsf_note_off(tsf* f, int preset, int key)
{
	struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
	for (; v != vEnd; v++)
		if (v->playingPreset == preset && v->playingKey == key)
			tsf_voice_end(v, f->outSampleRate);
}

TSFDEF void tsf_render_short(tsf* f, short* buffer, int samples, int flag_mixing)
{
	float *floatSamples;
	int channelSamples = (f->outputmode == TSF_MONO ? 1 : 2) * samples, floatBufferSize = channelSamples * sizeof(float);
	short* bufferEnd = buffer + channelSamples;
	if (floatBufferSize > f->outputSampleSize)
	{
		TSF_FREE(f->outputSamples);
		f->outputSamples = (float*)TSF_MALLOC(floatBufferSize);
		f->outputSampleSize = floatBufferSize;
	}

	tsf_render_float(f, f->outputSamples, samples, TSF_FALSE);

	floatSamples = f->outputSamples;
	if (flag_mixing) 
		while (buffer != bufferEnd)
		{
			float v = *floatSamples++;
			int vi = *buffer + (v < -1.00004566f ? (int)-32768 : (v > 1.00001514f ? (int)32767 : (int)(v * 32767.5f)));
			*buffer++ = (vi < -32768 ? (short)-32768 : (vi > 32767 ? (short)32767 : (short)vi));
		}
	else
		while (buffer != bufferEnd)
		{
			float v = *floatSamples++;
			*buffer++ = (v < -1.00004566f ? (short)-32768 : (v > 1.00001514f ? (short)32767 : (short)(v * 32767.5f)));
		}
}

TSFDEF void tsf_render_float(tsf* f, float* buffer, int samples, int flag_mixing)
{
	struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
	if (!flag_mixing) TSF_MEMSET(buffer, 0, (f->outputmode == TSF_MONO ? 1 : 2) * sizeof(float) * samples);
	for (; v != vEnd; v++)
		if (v->playingPreset != -1)
			tsf_voice_render(f, v, buffer, samples);
}

TSFDEF void tsf_render_short_fast(tsf* f, short* buffer, int samples, int flag_mixing)
{
  struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
  if (!flag_mixing) TSF_MEMSET(buffer, 0, (f->outputmode == TSF_MONO ? 1 : 2) * sizeof(short) * samples);
  for (; v != vEnd; v++) {
    if (v->playingPreset != -1)
      tsf_voice_render_fast(f, v, buffer, samples);
    yield();
  }
}



#ifdef __cplusplus
}
#endif

#endif //TSF_IMPLEMENTATION
