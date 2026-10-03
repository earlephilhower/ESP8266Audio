// Host-side check for AudioFileSourceID3 frame-size parsing.
// Fixtures are synthetic (tests/host/id3fix/gen_id3_fixtures.py).
//
// The pre-fix decoder treats an ID3v2.4 frame size as a plain uint32.
// A size byte with the high bit clear can still decode to a huge length
// (00 00 01 00 is 128 synchsafe, 256 plain). The fixture then plants a
// later header whose plain size is 0x20000000, which the old skip loop
// would walk. This file runs that old walk with a cap, then runs the
// library and requires it to finish the tag.

#include <Arduino.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "AudioFileSourceID3.h"
#include "AudioFileSourceSTDIO.h"

static int g_fail = 0;
static bool g_eof = false;
static char g_title[64];
static char g_performer[64];

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    g_fail = 1;
}

static void on_meta(void *cbData, const char *type, bool isUnicode, const char *string) {
    (void)cbData;
    (void)isUnicode;
    if (!strcmp(type, "eof")) {
        g_eof = true;
        return;
    }
    if (!strcmp(type, "Title")) {
        strncpy(g_title, string, sizeof(g_title) - 1);
    } else if (!strcmp(type, "Performer")) {
        strncpy(g_performer, string, sizeof(g_performer) - 1);
    }
}

static void reset_meta() {
    g_eof = false;
    memset(g_title, 0, sizeof(g_title));
    memset(g_performer, 0, sizeof(g_performer));
}

static void on_alarm(int sig) {
    (void)sig;
    fprintf(stderr, "FAIL: ID3 parse did not finish (skip loop ran away)\n");
    _exit(2);
}

// Walk tag bytes the way AudioFileSourceID3 did before the v2.4 split:
// rev 3 and rev 4 both take a plain 32-bit frame size, and the content
// loop does not stop at the tag. Stops after `cap` content-loop steps.
static uint32_t simulate_old(const uint8_t *file, int filelen, uint64_t cap,
                             bool *hit_cap, bool *finished) {
    *hit_cap = false;
    *finished = false;
    uint32_t maxfs = 0;
    if (filelen < 10 || memcmp(file, "ID3", 3) != 0) {
        return 0;
    }
    int rev = file[3];
    int end = 10 + ((file[6] << 21) | (file[7] << 14) | (file[8] << 7) | file[9]);
    if (end > filelen) {
        end = filelen;
    }
    int pos = 10;
    uint64_t spins = 0;
    while (pos < end) {
        unsigned char id[4];
        if (pos >= end) {
            break;
        }
        id[0] = file[pos++];
        id[1] = (pos < end) ? file[pos++] : 0;
        id[2] = (pos < end) ? file[pos++] : 0;
        id[3] = (rev == 2) ? 0 : ((pos < end) ? file[pos++] : 0);
        if (id[0] == 0 && id[1] == 0 && id[2] == 0 && id[3] == 0) {
            pos = end;
            break;
        }
        int framesize;
        if (rev == 2) {
            int b0 = (pos < end) ? file[pos++] : 0;
            int b1 = (pos < end) ? file[pos++] : 0;
            int b2 = (pos < end) ? file[pos++] : 0;
            framesize = (b0 << 16) | (b1 << 8) | b2;
        } else {
            int b0 = (pos < end) ? file[pos++] : 0;
            int b1 = (pos < end) ? file[pos++] : 0;
            int b2 = (pos < end) ? file[pos++] : 0;
            int b3 = (pos < end) ? file[pos++] : 0;
            framesize = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
            if (pos < end) {
                pos++;
            }
            if (pos < end) {
                pos++;
            }
        }
        if ((uint32_t)framesize > maxfs) {
            maxfs = (uint32_t)framesize;
        }
        // encoding byte + (framesize - 1) payload bytes, same as the old loop
        uint32_t need = (uint32_t)framesize;
        for (uint32_t i = 0; i < need; i++) {
            spins++;
            if (spins >= cap) {
                *hit_cap = true;
                return maxfs;
            }
            if (pos < end) {
                pos++;
            }
        }
    }
    *finished = true;
    return maxfs;
}

static int load(const char *path, uint8_t *buf, int cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FAIL: cannot open %s\n", path);
        g_fail = 1;
        return -1;
    }
    int n = (int)fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

static void parse_library(const char *path, uint8_t *audio, uint32_t *nread, uint32_t *pos) {
    reset_meta();
    AudioFileSourceSTDIO in(path);
    if (!in.isOpen()) {
        fail("open failed");
        *nread = 0;
        *pos = 0;
        return;
    }
    AudioFileSourceID3 id3(&in);
    id3.RegisterMetadataCB(on_meta, NULL);
    *nread = id3.read(audio, 16);
    *pos = in.getPos();
}

int main() {
    uint8_t file[1024];
    uint8_t audio[16];
    const uint64_t cap = 100000;

    int n = load("id3fix/large-v24.mp3", file, sizeof(file));
    if (n > 0) {
        bool hit = false, done = false;
        uint32_t maxfs = simulate_old(file, n, cap, &hit, &done);
        printf("old large-v24: max_plain_framesize=%u hit_cap=%d finished=%d\n", maxfs, hit, done);
        if (!hit || done || maxfs < 0x20000000u) {
            fail("old decoder should run away on large-v24");
        }
        signal(SIGALRM, on_alarm);
        alarm(2);
        uint32_t nread = 0, pos = 0;
        parse_library("id3fix/large-v24.mp3", audio, &nread, &pos);
        alarm(0);
        printf("lib large-v24: title='%.16s' performer='%s' eof=%d nread=%u pos=%u audio=%02x%02x\n",
               g_title, g_performer, g_eof, nread, pos, audio[0], audio[1]);
        if (!g_eof || g_title[0] != 'A' || strcmp(g_performer, "X") != 0) {
            fail("large-v24 tag was not walked");
        }
        if (nread < 2 || audio[0] != 0xFF || audio[1] != 0xFB) {
            fail("large-v24 did not reach the mpeg frame");
        }
    }

    n = load("id3fix/small-v24.mp3", file, sizeof(file));
    if (n > 0) {
        bool hit = false, done = false;
        uint32_t maxfs = simulate_old(file, n, cap, &hit, &done);
        printf("old small-v24: max_plain_framesize=%u hit_cap=%d finished=%d\n", maxfs, hit, done);
        if (hit || !done || maxfs >= 128) {
            fail("small-v24 should be safe for the old decoder");
        }
        uint32_t nread = 0, pos = 0;
        parse_library("id3fix/small-v24.mp3", audio, &nread, &pos);
        printf("lib small-v24: title='%s' eof=%d audio=%02x%02x\n", g_title, g_eof, audio[0], audio[1]);
        if (!g_eof || strcmp(g_title, "Hi") != 0) {
            fail("small-v24 title mismatch");
        }
        if (nread < 2 || audio[0] != 0xFF || audio[1] != 0xFB) {
            fail("small-v24 did not reach the mpeg frame");
        }
    }

    n = load("id3fix/large-v23.mp3", file, sizeof(file));
    if (n > 0) {
        bool hit = false, done = false;
        uint32_t maxfs = simulate_old(file, n, cap, &hit, &done);
        printf("old large-v23: max_plain_framesize=%u hit_cap=%d finished=%d\n", maxfs, hit, done);
        if (hit || !done || maxfs != 256) {
            fail("v2.3 plain size 256 should finish under the old decoder");
        }
        uint32_t nread = 0, pos = 0;
        parse_library("id3fix/large-v23.mp3", audio, &nread, &pos);
        printf("lib large-v23: title='%.8s' performer='%s' eof=%d audio=%02x%02x\n",
               g_title, g_performer, g_eof, audio[0], audio[1]);
        if (!g_eof || g_title[0] != 'B' || strcmp(g_performer, "Q") != 0) {
            fail("v2.3 large frame was not read as a plain size");
        }
        if (nread < 2 || audio[0] != 0xFF || audio[1] != 0xFB) {
            fail("large-v23 did not reach the mpeg frame");
        }
    }

    n = load("id3fix/truncated-v24.mp3", file, sizeof(file));
    if (n > 0) {
        bool hit = false, done = false;
        uint32_t maxfs = simulate_old(file, n, cap, &hit, &done);
        printf("old truncated-v24: max_plain_framesize=%u hit_cap=%d finished=%d\n", maxfs, hit, done);
        if (!hit) {
            fail("truncated v2.4 size should blow the old skip loop");
        }
        signal(SIGALRM, on_alarm);
        alarm(2);
        uint32_t nread = 0, pos = 0;
        parse_library("id3fix/truncated-v24.mp3", audio, &nread, &pos);
        alarm(0);
        printf("lib truncated-v24: eof=%d nread=%u audio=%02x%02x\n", g_eof, nread, audio[0], audio[1]);
        if (!g_eof) {
            fail("truncated-v24 did not reach tag end");
        }
        if (nread < 2 || audio[0] != 0xFF || audio[1] != 0xFB) {
            fail("truncated-v24 did not reach the mpeg frame");
        }
    }

    if (g_fail) {
        fprintf(stderr, "id3 tests failed\n");
        return 1;
    }
    printf("id3 tests passed\n");
    return 0;
}
