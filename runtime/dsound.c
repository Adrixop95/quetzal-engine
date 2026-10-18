// IDirectSound / IDirectSoundBuffer software mixer on SDL audio.
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"

enum { DS_OK = 0 };

typedef struct {
  int primary;
  uint32_t data, bytes;          // guest memory
  int channels, bits, rate;
  uint32_t pos;                  // play cursor, bytes
  double frac;
  int playing, looping;
  int vol_mb;                    // hundredths of dB (<= 0)
  int pan;
  int freq;                      // 0 = native
} SndBuf;

#define MAX_BUFS 256
static SndBuf* bufs[MAX_BUFS];
static SDL_AudioDeviceID audio_dev;
static int mix_rate = 44100;

static void audio_cb(void* ud, uint8_t* stream, int len) {
  (void)ud;
  int16_t* out = (int16_t*)stream;
  int frames = len / 4;
  memset(stream, 0, (size_t)len);
  for (int b = 0; b < MAX_BUFS; b++) {
    SndBuf* s = bufs[b];
    if (!s || !s->playing || s->primary || !s->bytes) continue;
    double gain = pow(10.0, s->vol_mb / 2000.0);
    double lg = gain, rg = gain;
    if (s->pan > 0) lg *= pow(10.0, -s->pan / 2000.0);
    if (s->pan < 0) rg *= pow(10.0, s->pan / 2000.0);
    int rate = s->freq ? s->freq : s->rate;
    double step = (double)rate / mix_rate;
    uint32_t frame_bytes = (uint32_t)(s->channels * s->bits / 8);
    const uint8_t* mem = g_mem + s->data;
    for (int i = 0; i < frames && s->playing; i++) {
      uint32_t fpos = (uint32_t)s->frac;
      uint32_t off = s->pos + fpos * frame_bytes;
      if (off + frame_bytes > s->bytes) {
        if (s->looping) { s->pos = 0; s->frac = 0; off = 0; } else { s->playing = 0; break; }
      }
      int l, r;
      if (s->bits == 16) {
        const int16_t* p = (const int16_t*)(mem + off);
        l = p[0]; r = s->channels == 2 ? p[1] : p[0];
      } else {
        l = ((int)mem[off] - 128) << 8; r = s->channels == 2 ? ((int)mem[off + 1] - 128) << 8 : l;
      }
      int ol = out[2 * i] + (int)(l * lg), orr = out[2 * i + 1] + (int)(r * rg);
      out[2 * i] = (int16_t)(ol > 32767 ? 32767 : ol < -32768 ? -32768 : ol);
      out[2 * i + 1] = (int16_t)(orr > 32767 ? 32767 : orr < -32768 ? -32768 : orr);
      s->frac += step;
      if (s->frac >= 1.0) {
        uint32_t adv = (uint32_t)s->frac;
        s->pos += adv * frame_bytes;
        s->frac -= adv;
        if (s->pos + frame_bytes > s->bytes) {
          if (s->looping) s->pos = 0; else { s->playing = 0; s->pos = 0; }
        }
      }
    }
  }
}

static void ensure_audio(void) {
  if (audio_dev) return;
  if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
  SDL_AudioSpec want = {0}, have;
  want.freq = mix_rate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
  audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev) { mix_rate = have.freq; SDL_PauseAudioDevice(audio_dev, 0); }
  else rt_log("audio disabled: %s", SDL_GetError());
}

static uint32_t com_addref(CPU* c, uint32_t* a) { (void)c; (void)a; return 2; }
static uint32_t com_release(CPU* c, uint32_t* a) {
  (void)c;
  SndBuf* s = com_host(a[0]);
  if (s && !s->primary) {  // buffers are released by the game when done
    if (audio_dev) SDL_LockAudioDevice(audio_dev);
    s->playing = 0;
    for (int i = 0; i < MAX_BUFS; i++) if (bufs[i] == s) bufs[i] = NULL;
    if (audio_dev) SDL_UnlockAudioDevice(audio_dev);
  }
  return 0;
}
static uint32_t com_qi(CPU* c, uint32_t* a) { (void)c; GU32(a[2]) = a[0]; return DS_OK; }
static uint32_t ds_ok(CPU* c, uint32_t* a) { (void)c; (void)a; return DS_OK; }

#define LOCKED(s) if (audio_dev) SDL_LockAudioDevice(audio_dev); 
#define UNLOCKED() if (audio_dev) SDL_UnlockAudioDevice(audio_dev);

static uint32_t buf_getcurrentpos(CPU* c, uint32_t* a) {
  (void)c;
  SndBuf* s = com_host(a[0]);
  if (a[1]) GU32(a[1]) = s->pos;
  if (a[2]) GU32(a[2]) = s->bytes ? (s->pos + 2048) % s->bytes : 0;
  return DS_OK;
}
static uint32_t buf_getformat(CPU* c, uint32_t* a) { (void)c; (void)a; return DS_OK; }
static uint32_t buf_getvolume(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); GU32(a[1]) = (uint32_t)s->vol_mb; return DS_OK; }
static uint32_t buf_getpan(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); GU32(a[1]) = (uint32_t)s->pan; return DS_OK; }
static uint32_t buf_getfreq(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); GU32(a[1]) = (uint32_t)(s->freq ? s->freq : s->rate); return DS_OK; }
static uint32_t buf_getstatus(CPU* c, uint32_t* a) {
  (void)c;
  SndBuf* s = com_host(a[0]);
  GU32(a[1]) = (s->playing ? 1u : 0u) | (s->playing && s->looping ? 4u : 0u);
  return DS_OK;
}
static uint32_t buf_lock(CPU* c, uint32_t* a) {  // off, bytes, p1*, b1*, p2*, b2*, flags
  (void)c;
  SndBuf* s = com_host(a[0]);
  uint32_t off = a[1], bytes = a[2];
  if (a[7] & 2) bytes = s->bytes;       // DSBLOCK_ENTIREBUFFER
  if (a[7] & 1) off = s->pos;           // DSBLOCK_FROMWRITECURSOR
  if (off > s->bytes) off = 0;
  uint32_t first = bytes;
  if (off + first > s->bytes) first = s->bytes - off;
  GU32(a[3]) = s->data + off; GU32(a[4]) = first;
  if (a[5]) GU32(a[5]) = first < bytes ? s->data : 0;
  if (a[6]) GU32(a[6]) = first < bytes ? bytes - first : 0;
  return DS_OK;
}
static uint32_t buf_play(CPU* c, uint32_t* a) {  // res, prio, flags
  (void)c;
  SndBuf* s = com_host(a[0]);
  ensure_audio();
  LOCKED(s)
  s->playing = 1; s->looping = (a[3] & 1) != 0;
  UNLOCKED()
  return DS_OK;
}
static uint32_t buf_stop(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); LOCKED(s) s->playing = 0; UNLOCKED() return DS_OK; }
static uint32_t buf_setpos(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); LOCKED(s) s->pos = a[1] % (s->bytes ? s->bytes : 1); s->frac = 0; UNLOCKED() return DS_OK; }
static uint32_t buf_setvol(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); s->vol_mb = (int)a[1]; return DS_OK; }
static uint32_t buf_setpan(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); s->pan = (int)a[1]; return DS_OK; }
static uint32_t buf_setfreq(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); s->freq = a[1] == 0 || a[1] == 0 ? 0 : (int)a[1]; return DS_OK; }
static uint32_t buf_getcaps(CPU* c, uint32_t* a) { (void)c; SndBuf* s = com_host(a[0]); memset(GPTR(a[1] + 4), 0, 16); GU32(a[1]) = 20; GU32(a[1] + 8) = s->bytes; return DS_OK; }

static const ComMethod buf_methods[] = {
  {"QueryInterface", 2, com_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release}, {"GetCaps", 1, buf_getcaps},
  {"GetCurrentPosition", 2, buf_getcurrentpos}, {"GetFormat", 3, buf_getformat}, {"GetVolume", 1, buf_getvolume},
  {"GetPan", 1, buf_getpan}, {"GetFrequency", 1, buf_getfreq}, {"GetStatus", 1, buf_getstatus}, {"Initialize", 2, NULL},
  {"Lock", 7, buf_lock}, {"Play", 3, buf_play}, {"SetCurrentPosition", 1, buf_setpos}, {"SetFormat", 1, ds_ok},
  {"SetVolume", 1, buf_setvol}, {"SetPan", 1, buf_setpan}, {"SetFrequency", 1, buf_setfreq}, {"Stop", 0, buf_stop},
  {"Unlock", 4, ds_ok}, {"Restore", 0, ds_ok},
};

static uint32_t ds_createbuffer(CPU* c, uint32_t* a) {  // desc*, **buf, outer
  (void)c;
  uint32_t d = a[1];
  SndBuf* s = calloc(1, sizeof *s);
  uint32_t flags = GU32(d + 4);
  s->primary = (flags & 1) != 0;
  s->bytes = GU32(d + 8);
  uint32_t wf = GU32(d + 16);
  s->channels = 2; s->bits = 16; s->rate = 22050;
  if (wf) { s->channels = GU16(wf + 2); s->rate = (int)GU32(wf + 4); s->bits = GU16(wf + 14); }
  if (!s->primary) {
    s->data = gmalloc(s->bytes);
    memset(GPTR(s->data), s->bits == 8 ? 0x80 : 0, s->bytes);
    if (audio_dev) SDL_LockAudioDevice(audio_dev);
    for (int i = 0; i < MAX_BUFS; i++) if (!bufs[i]) { bufs[i] = s; break; }
    if (audio_dev) SDL_UnlockAudioDevice(audio_dev);
  }
  GU32(a[2]) = com_create(buf_methods, 21, s);
  return DS_OK;
}
static uint32_t ds_getcaps(CPU* c, uint32_t* a) { (void)c; memset(GPTR(a[1] + 4), 0, GU32(a[1]) - 4); GU32(a[1] + 4) = 0x1 | 0x2 | 0x10 | 0x20; return DS_OK; }
static uint32_t ds_dup(CPU* c, uint32_t* a) {  // src buffer, **dst
  (void)c;
  SndBuf* src = com_host(a[1]);
  SndBuf* s = calloc(1, sizeof *s);
  *s = *src;
  s->playing = 0; s->pos = 0; s->frac = 0;
  s->data = gmalloc(s->bytes);
  memcpy(GPTR(s->data), GPTR(src->data), s->bytes);
  if (audio_dev) SDL_LockAudioDevice(audio_dev);
  for (int i = 0; i < MAX_BUFS; i++) if (!bufs[i]) { bufs[i] = s; break; }
  if (audio_dev) SDL_UnlockAudioDevice(audio_dev);
  GU32(a[2]) = com_create(buf_methods, 21, s);
  return DS_OK;
}
static const ComMethod ds_methods[] = {
  {"QueryInterface", 2, com_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release},
  {"CreateSoundBuffer", 3, ds_createbuffer}, {"GetCaps", 1, ds_getcaps}, {"DuplicateSoundBuffer", 2, ds_dup},
  {"SetCooperativeLevel", 2, ds_ok}, {"Compact", 0, ds_ok}, {"GetSpeakerConfig", 1, NULL}, {"SetSpeakerConfig", 1, ds_ok},
  {"Initialize", 1, ds_ok},
};

static uint32_t sh_DirectSoundCreate(CPU* c, uint32_t* a) {  // guid, **ds, outer
  (void)c;
  ensure_audio();
  GU32(a[1]) = com_create(ds_methods, 11, NULL);
  return DS_OK;
}

void rt_register_dsound_shims(void) {
  static const Shim s[] = {{"DSOUND.dll", "#1", 3, sh_DirectSoundCreate}};
  rt_register_shims(s, 1);
}
