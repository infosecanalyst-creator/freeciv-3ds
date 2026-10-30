// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 the freeciv-3ds authors
#include "audio.h"
#include "sound_table.h"
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// All assets are IMA ADPCM WAVs (mono, 22050 Hz), made by tools/make_audio.sh.

static bool s_ok;
static int s_music_vol = 6, s_sfx_vol = 8;
static LightLock s_lock;
static void music_update(void);

// ---------------------------------------------------------------- IMA ADPCM
static const int k_ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767 };
static const int k_ima_index[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

// Decodes one mono block (4-byte header + nibbles) into out; returns samples.
static int ima_block(const u8 *in, int len, s16 *out)
{
    if (len < 4) return 0;
    int pred = (s16)(in[0] | (in[1] << 8));
    int idx = in[2];
    if (idx > 88) idx = 88;
    int n = 0;
    out[n++] = (s16)pred;
    for (int i = 4; i < len; i++) {
        for (int half = 0; half < 2; half++) {
            int nib = half ? (in[i] >> 4) : (in[i] & 15);
            int step = k_ima_steps[idx];
            int diff = step >> 3;
            if (nib & 1) diff += step >> 2;
            if (nib & 2) diff += step >> 1;
            if (nib & 4) diff += step;
            if (nib & 8) pred -= diff; else pred += diff;
            if (pred > 32767) pred = 32767;
            if (pred < -32768) pred = -32768;
            idx += k_ima_index[nib];
            if (idx < 0) idx = 0;
            if (idx > 88) idx = 88;
            out[n++] = (s16)pred;
        }
    }
    return n;
}

typedef struct {
    FILE *f;
    int block_align, rate;
    long data_start, data_len, data_pos;
} ImaFile;

static bool ima_open(ImaFile *w, const char *path)
{
    memset(w, 0, sizeof(*w));
    w->f = fopen(path, "rb");
    // Big reads, rarely: the engine reads rulesets/saves from the same media,
    // and many 1 KB reads per frame queue behind it and stutter.
    if (w->f) setvbuf(w->f, NULL, _IOFBF, 32 * 1024);
    if (!w->f) return false;
    u8 h[12];
    if (fread(h, 1, 12, w->f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) goto fail;
    for (;;) {
        u8 c[8];
        if (fread(c, 1, 8, w->f) != 8) goto fail;
        u32 sz = c[4] | (c[5] << 8) | (c[6] << 16) | ((u32)c[7] << 24);
        if (!memcmp(c, "fmt ", 4)) {
            u8 fmt[20] = {0};
            if (fread(fmt, 1, sz < 20 ? sz : 20, w->f) < 16) goto fail;
            if (sz > 20) fseek(w->f, sz - 20, SEEK_CUR);
            if ((fmt[0] | (fmt[1] << 8)) != 0x11 || (fmt[2] | (fmt[3] << 8)) != 1) goto fail;
            w->rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16);
            w->block_align = fmt[12] | (fmt[13] << 8);
        } else if (!memcmp(c, "data", 4)) {
            w->data_start = ftell(w->f);
            w->data_len = sz;
            break;
        } else {
            fseek(w->f, sz + (sz & 1), SEEK_CUR);
        }
    }
    if (w->block_align < 8) goto fail;
    return true;
fail:
    fclose(w->f);
    w->f = NULL;
    return false;
}

static int samples_per_block(const ImaFile *w) { return 1 + (w->block_align - 4) * 2; }

// Next block into out (>= samples_per_block); 0 at end.
static int ima_next(ImaFile *w, s16 *out)
{
    static u8 buf[4096];
    long left = w->data_len - w->data_pos;
    if (left <= 0) return 0;
    int n = left < w->block_align ? (int)left : w->block_align;
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    if ((int)fread(buf, 1, n, w->f) != n) return 0;
    w->data_pos += n;
    return ima_block(buf, n, out);
}

static void ima_rewind(ImaFile *w)
{
    fseek(w->f, w->data_start, SEEK_SET);
    w->data_pos = 0;
}

static void ima_close(ImaFile *w)
{
    if (w->f) fclose(w->f);
    w->f = NULL;
}

// ---------------------------------------------------------------- effects
#define SFX_CACHE 24
#define SFX_FIRST_CH 1
#define SFX_CHANNELS 6
typedef struct { char file[24]; s16 *pcm; int samples; int rate; } Sfx;
static Sfx s_cache[SFX_CACHE];
static int s_ncache;
static ndspWaveBuf s_sfx_buf[SFX_CHANNELS];

static Sfx *load_sfx(const char *file)
{
    for (int i = 0; i < s_ncache; i++) if (!strcmp(s_cache[i].file, file)) return &s_cache[i];
    if (s_ncache >= SFX_CACHE) return NULL;
    char path[96];
    snprintf(path, sizeof(path), "romfs:/audio/sfx/%s.wav", file);
    ImaFile w;
    if (!ima_open(&w, path)) return NULL;
    int spb = samples_per_block(&w);
    int blocks = (int)((w.data_len + w.block_align - 1) / w.block_align);
    s16 *pcm = linearAlloc((size_t)blocks * spb * sizeof(s16));
    if (!pcm) { ima_close(&w); return NULL; }
    int total = 0, n;
    while ((n = ima_next(&w, pcm + total)) > 0) total += n;
    ima_close(&w);
    DSP_FlushDataCache(pcm, total * sizeof(s16));
    // Fill the entry first, then publish it under the lock (audio_play reads
    // the cache from the main thread).
    Sfx *s = &s_cache[s_ncache];
    snprintf(s->file, sizeof(s->file), "%s", file);
    s->pcm = pcm; s->samples = total; s->rate = w.rate;
    LightLock_Lock(&s_lock);
    s_ncache++;
    LightLock_Unlock(&s_lock);
    return s;
}

static const char *lookup(const char *tag)
{
    for (unsigned i = 0; i < sizeof(k_sound_table) / sizeof(k_sound_table[0]); i++)
        if (!strcasecmp(k_sound_table[i].tag, tag)) return k_sound_table[i].file;
    return NULL;
}

static void play_sfx(Sfx *s);

// Sound effects not yet in memory are decoded on the audio thread, not here.
#define SFX_QUEUE 8
static const char *s_sfx_queue[SFX_QUEUE];
static int s_sfx_qn;
static Thread s_thread;
static volatile bool s_thread_run;

void audio_play(const char *tag)
{
    if (!s_ok || s_sfx_vol <= 0 || !tag) return;
    const char *file = lookup(tag);
    if (!file && (tag[0] == 'm' || tag[0] == 'f') && tag[1] == '_') {
        char gen[16];
        snprintf(gen, sizeof(gen), "%c_generic", tag[0]);
        file = lookup(gen);
    }
    if (!file) return;
    LightLock_Lock(&s_lock);
    Sfx *s = NULL;
    for (int i = 0; i < s_ncache; i++) if (!strcmp(s_cache[i].file, file)) s = &s_cache[i];
    if (s) play_sfx(s);
    else if (s_sfx_qn < SFX_QUEUE) s_sfx_queue[s_sfx_qn++] = file; // table strings are static
    LightLock_Unlock(&s_lock);
}

static void play_sfx(Sfx *s)
{
    int ch = -1;
    for (int i = 0; i < SFX_CHANNELS; i++) {
        ndspWaveBuf *b = &s_sfx_buf[i];
        if (b->status == NDSP_WBUF_FREE || b->status == NDSP_WBUF_DONE) { ch = i; break; }
    }
    if (ch < 0) return;
    int hw = SFX_FIRST_CH + ch;
    ndspChnReset(hw);
    ndspChnSetInterp(hw, NDSP_INTERP_LINEAR);
    ndspChnSetRate(hw, (float)s->rate);
    ndspChnSetFormat(hw, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0};
    mix[0] = mix[1] = s_sfx_vol / 10.0f;
    ndspChnSetMix(hw, mix);
    ndspWaveBuf *b = &s_sfx_buf[ch];
    memset(b, 0, sizeof(*b));
    b->data_vaddr = s->pcm;
    b->nsamples = s->samples;
    ndspChnWaveBufAdd(hw, b);
}

// ---------------------------------------------------------------- music
#define MUSIC_CH 0
#define MUSIC_BUFS 8          // ~3 s queued, so a busy moment never starves it
#define MUSIC_BUF_SAMPLES (2041 * 4)
static s16 *s_mbuf[MUSIC_BUFS];
static ndspWaveBuf s_mwave[MUSIC_BUFS];
static ImaFile s_mfile;
static MusicMode s_mode = MUSIC_NONE;
static int s_track = -1;
static int s_gap_frames;          // silence between game tracks
static const char *k_game_tracks[] = { "exploration", "harvest_season", "market_day", "bards_tale" };
#define N_GAME_TRACKS ((int)(sizeof(k_game_tracks) / sizeof(k_game_tracks[0])))

static void music_mix(void)
{
    float mix[12] = {0};
    mix[0] = mix[1] = s_music_vol / 10.0f * 0.7f;
    ndspChnSetMix(MUSIC_CH, mix);
}

static bool music_open_next(void)
{
    ima_close(&s_mfile);
    char path[96];
    if (s_mode == MUSIC_MENU) {
        snprintf(path, sizeof(path), "romfs:/audio/music/menu.wav");
    } else {
        int next = s_track;
        if (N_GAME_TRACKS > 1) while (next == s_track) next = rand() % N_GAME_TRACKS;
        else next = 0;
        s_track = next;
        snprintf(path, sizeof(path), "romfs:/audio/music/%s.wav", k_game_tracks[s_track]);
    }
    if (!ima_open(&s_mfile, path)) return false;
    ndspChnReset(MUSIC_CH);
    ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
    ndspChnSetRate(MUSIC_CH, (float)s_mfile.rate);
    ndspChnSetFormat(MUSIC_CH, NDSP_FORMAT_MONO_PCM16);
    music_mix();
    for (int i = 0; i < MUSIC_BUFS; i++) s_mwave[i].status = NDSP_WBUF_DONE;
    return true;
}

static int music_fill(s16 *dst)
{
    int total = 0;
    int spb = samples_per_block(&s_mfile);
    while (total + spb <= MUSIC_BUF_SAMPLES) {
        int n = ima_next(&s_mfile, dst + total);
        if (n <= 0) {
            if (s_mode == MUSIC_MENU) { ima_rewind(&s_mfile); continue; }  // menu loops
            break;
        }
        total += n;
    }
    return total;
}

// Music state belongs to whoever runs music_update() (the audio thread);
// the main thread only posts the wanted mode, so it never waits on romfs.
static volatile int s_req_mode = MUSIC_NONE;

static void music_apply_mode(void)
{
    MusicMode mode = (MusicMode)s_req_mode;
    if (mode == s_mode) return;
    s_mode = mode;
    ndspChnWaveBufClear(MUSIC_CH);
    ima_close(&s_mfile);
    s_gap_frames = mode == MUSIC_GAME ? 90 : 0;
    // A track that won't open leaves s_mode as is (silence), rather than
    // resetting to NONE and being retried every frame.
    if (mode == MUSIC_MENU && !music_open_next()) ima_close(&s_mfile);
}

void audio_set_music(MusicMode mode)
{
    if (!s_ok || mode == (MusicMode)s_req_mode) return;
    s_req_mode = mode;
    if (!s_thread) music_apply_mode();
}

// The audio thread: feeds the music and decodes new sound effects, so the
// game's main loop never waits on the SD card / romfs for audio.
static void audio_thread(void *arg)
{
    (void)arg;
    while (s_thread_run) {
        music_apply_mode();
        music_update();     // file I/O, outside the lock
        // Decode queued effects outside the lock (slow), then play them.
        for (;;) {
            LightLock_Lock(&s_lock);
            const char *file = s_sfx_qn > 0 ? s_sfx_queue[0] : NULL;
            if (file) memmove(s_sfx_queue, s_sfx_queue + 1, (--s_sfx_qn) * sizeof(s_sfx_queue[0]));
            LightLock_Unlock(&s_lock);
            if (!file) break;
            Sfx *s = load_sfx(file);
            if (s) { LightLock_Lock(&s_lock); play_sfx(s); LightLock_Unlock(&s_lock); }
        }
        svcSleepThread(15 * 1000 * 1000ULL); // 15 ms
    }
}

void audio_update(void)
{
    // The audio thread does this; only if it couldn't start, do it here.
    if (!s_ok || s_thread) return;
    music_update();
    LightLock_Lock(&s_lock);
    const char *file = s_sfx_qn > 0 ? s_sfx_queue[--s_sfx_qn] : NULL;
    LightLock_Unlock(&s_lock);
    if (file) {
        Sfx *s = load_sfx(file);
        if (s) { LightLock_Lock(&s_lock); play_sfx(s); LightLock_Unlock(&s_lock); }
    }
}

static void music_update(void)
{
    if (!s_ok || s_mode == MUSIC_NONE) return;
    if (!s_mfile.f) {
        if (s_gap_frames > 0) { s_gap_frames--; return; }
        if (!music_open_next()) { s_gap_frames = 600; return; } // try again later, quietly
    }
    bool ended = false;
    for (int i = 0; i < MUSIC_BUFS; i++) {
        ndspWaveBuf *b = &s_mwave[i];
        if (b->status != NDSP_WBUF_DONE && b->status != NDSP_WBUF_FREE) continue;
        int n = music_fill(s_mbuf[i]);
        if (n <= 0) { ended = true; break; }
        DSP_FlushDataCache(s_mbuf[i], n * sizeof(s16));
        memset(b, 0, sizeof(*b));
        b->data_vaddr = s_mbuf[i];
        b->nsamples = n;
        ndspChnWaveBufAdd(MUSIC_CH, b);
    }
    if (ended) {
        // Let the queued buffers drain, then a short gap before the next track.
        bool playing = false;
        for (int i = 0; i < MUSIC_BUFS; i++)
            if (s_mwave[i].status == NDSP_WBUF_QUEUED || s_mwave[i].status == NDSP_WBUF_PLAYING) playing = true;
        if (!playing) {
            ima_close(&s_mfile);
            s_gap_frames = 180;
        }
    }
}

// ---------------------------------------------------------------- setup
void audio_init(void)
{
    if (R_FAILED(ndspInit())) {
        printf("audio: ndspInit failed (no /3ds/dspfirm.cdc?) - sound off\n");
        return;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for (int i = 0; i < MUSIC_BUFS; i++) {
        s_mbuf[i] = linearAlloc(MUSIC_BUF_SAMPLES * sizeof(s16));
        if (!s_mbuf[i]) { ndspExit(); return; }
    }
    s_ok = true;
    LightLock_Init(&s_lock);
    s_thread_run = true;
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // A little above the main thread so it gets its few ms when it needs
    // them; it sleeps the rest of the time.
    s_thread = threadCreate(audio_thread, NULL, 32 * 1024, prio - 1, -2, false);
    if (!s_thread) { s_thread_run = false; }
}

void audio_exit(void)
{
    if (!s_ok) return;
    if (s_thread) {
        s_thread_run = false;
        threadJoin(s_thread, 1000ULL * 1000 * 1000);
        threadFree(s_thread);
        s_thread = NULL;
    }
    ima_close(&s_mfile);
    for (int i = 0; i < 24; i++) ndspChnWaveBufClear(i);
    ndspExit();
    for (int i = 0; i < MUSIC_BUFS; i++) if (s_mbuf[i]) linearFree(s_mbuf[i]);
    for (int i = 0; i < s_ncache; i++) linearFree(s_cache[i].pcm);
    s_ncache = 0;
    s_ok = false;
}

void audio_set_volumes(int music, int sfx)
{
    s_music_vol = music < 0 ? 0 : music > 10 ? 10 : music;
    s_sfx_vol = sfx < 0 ? 0 : sfx > 10 ? 10 : sfx;
    if (s_ok) music_mix();
}
int audio_music_volume(void) { return s_music_vol; }
int audio_sfx_volume(void) { return s_sfx_vol; }
bool audio_available(void) { return s_ok; }
