#include "AudioBackend.h"
#include "PhxMemory.h"
#include "PhxMath.h"
#include "PhxString.h"
#include "SoundDef.h"

#include <SDL3/SDL.h>

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PHX_MAX_VOICES 64
#define PHX_AUDIO_CHANNELS 2
#define PHX_AUDIO_SAMPLE_RATE 44100
#define PHX_MIX_FRAMES 1024

static bool PathHasExt (cstr path, cstr ext) {
  size_t pathLen = StrLen(path);
  size_t extLen = StrLen(ext);
  if (pathLen < extLen) return false;
  return StrEqual(path + pathLen - extLen, ext);
}

struct AsyncLoadJob {
  SoundDesc* desc;
  cstr       path;
};

static int AsyncLoadThread (void* userdata) {
  AsyncLoadJob* job = (AsyncLoadJob*) userdata;
  bool ok = false;
  PCMBuffer* pcm = AudioBackend_LoadPCM(job->path, &ok);
  job->desc->pcm = pcm;
  job->desc->loadComplete = ok;
  job->desc->loadFailed = !ok;
  StrFree(job->path);
  MemFree(job);
  return 0;
}

struct Voice {
  PCMBuffer* pcm;
  bool       active;
  bool       paused;
  bool       looped;
  bool       is3D;
  bool       finished;
  float      volume;
  float      pitch;
  float      pan;
  float      level3D;
  Vec3f      pos;
  Vec3f      vel;
  double     playFrame;
};

static struct {
  SDL_AudioStream* stream;
  Voice            voices[PHX_MAX_VOICES];
  Vec3f            listenerPos;
  Vec3f            listenerVel;
  Vec3f            listenerFwd;
  Vec3f            listenerUp;
  float            doppler;
  float            distanceScale;
  float            rolloff;
  float            mixBuffer[PHX_MIX_FRAMES * PHX_AUDIO_CHANNELS];
} self;

static float Voice_ReadSample (Voice* v, int channel) {
  if (!v->pcm || v->pcm->frameCount == 0) return 0.0f;

  double framePos = v->playFrame;
  int frame = (int) framePos;
  float frac = (float) (framePos - frame);

  if (frame >= v->pcm->frameCount) {
    if (v->looped) {
      frame = frame % v->pcm->frameCount;
      v->playFrame = (double) frame;
    } else {
      v->finished = true;
      return 0.0f;
    }
  }

  int ch = channel;
  if (ch >= v->pcm->channels) ch = 0;

  int idx0 = frame * v->pcm->channels + ch;
  int idx1 = idx0 + v->pcm->channels;
  if (idx1 >= v->pcm->frameCount * v->pcm->channels)
    idx1 = v->looped ? (idx1 % (v->pcm->frameCount * v->pcm->channels)) : idx0;

  float s0 = v->pcm->samples[idx0];
  float s1 = v->pcm->samples[idx1];
  return s0 + (s1 - s0) * frac;
}

static void Voice_Advance (Voice* v, int frames) {
  v->playFrame += (double) frames * v->pitch;
  if (!v->looped && v->pcm && v->playFrame >= v->pcm->frameCount)
    v->finished = true;
}

static float Voice_ComputeGain (Voice* v) {
  float gain = v->volume;
  if (v->is3D) {
    Vec3f delta = Vec3f_Sub(v->pos, self.listenerPos);
    float dist = Vec3f_Length(delta);
    dist = Max(dist, 0.001f);
    gain *= 1.0f / (1.0f + self.rolloff * dist * self.distanceScale);
    gain *= v->level3D;
  }
  return gain;
}

static void Voice_ComputePan (Voice* v, float* panL, float* panR) {
  if (!v->is3D) {
    float p = Clamp(v->pan, -1.0f, 1.0f);
    *panL = (p <= 0.0f) ? 1.0f : 1.0f - p;
    *panR = (p >= 0.0f) ? 1.0f : 1.0f + p;
    return;
  }

  Vec3f delta = Vec3f_Sub(v->pos, self.listenerPos);
  if (Vec3f_Dot(delta, delta) < 1e-6f) {
    *panL = 1.0f;
    *panR = 1.0f;
    return;
  }

  Vec3f dir = Vec3f_Normalize(delta);
  Vec3f right = Vec3f_Normalize(Vec3f_Cross(self.listenerFwd, self.listenerUp));
  float dot = Vec3f_Dot(dir, right);
  dot = Clamp(dot, -1.0f, 1.0f);
  *panL = (dot <= 0.0f) ? 1.0f : 1.0f - dot;
  *panR = (dot >= 0.0f) ? 1.0f : 1.0f + dot;
}

static void AudioBackend_Mix () {
  MemZero(self.mixBuffer, sizeof(self.mixBuffer));

  for (int v = 0; v < PHX_MAX_VOICES; ++v) {
    Voice* voice = &self.voices[v];
    if (!voice->active || voice->paused || voice->finished || !voice->pcm)
      continue;

    float gain = Voice_ComputeGain(voice);
    float panL, panR;
    Voice_ComputePan(voice, &panL, &panR);

    for (int i = 0; i < PHX_MIX_FRAMES; ++i) {
      float sampleL = Voice_ReadSample(voice, 0);
      float sampleR = voice->pcm->channels > 1
        ? Voice_ReadSample(voice, 1)
        : sampleL;

      self.mixBuffer[i * 2 + 0] += sampleL * gain * panL;
      self.mixBuffer[i * 2 + 1] += sampleR * gain * panR;
      Voice_Advance(voice, 1);
    }
  }

  SDL_PutAudioStreamData(
    self.stream,
    self.mixBuffer,
    PHX_MIX_FRAMES * PHX_AUDIO_CHANNELS * (int) sizeof(float));
}

void AudioBackend_Init () {
  MemZero(&self, sizeof(self));
  self.listenerFwd = Vec3f_Create(0, 0, -1);
  self.listenerUp  = Vec3f_Create(0, 1, 0);
  self.doppler = 1.0f;
  self.distanceScale = 1.0f;
  self.rolloff = 1.0f;

  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_F32;
  spec.channels = PHX_AUDIO_CHANNELS;
  spec.freq = PHX_AUDIO_SAMPLE_RATE;

  self.stream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
  if (!self.stream)
    Fatal("AudioBackend_Init: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());

  SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(self.stream));
}

void AudioBackend_Free () {
  if (self.stream) {
    SDL_DestroyAudioStream(self.stream);
    self.stream = 0;
  }
  MemZero(self.voices, sizeof(self.voices));
}

void AudioBackend_Update () {
  if (self.stream)
    AudioBackend_Mix();
}

void AudioBackend_Set3DSettings (float doppler, float scale, float rolloff) {
  self.doppler = doppler;
  self.distanceScale = scale;
  self.rolloff = rolloff;
}

void AudioBackend_SetListenerPos (
  Vec3f const* pos,
  Vec3f const* vel,
  Vec3f const* fwd,
  Vec3f const* up)
{
  if (pos) self.listenerPos = *pos;
  if (vel) self.listenerVel = *vel;
  if (fwd) self.listenerFwd = *fwd;
  if (up)  self.listenerUp  = *up;
}

static PCMBuffer* PCMBuffer_Create (float* samples, int frameCount, int channels, int sampleRate) {
  PCMBuffer* pcm = MemNew(PCMBuffer);
  pcm->samples = samples;
  pcm->frameCount = frameCount;
  pcm->channels = channels;
  pcm->sampleRate = sampleRate;
  pcm->duration = sampleRate > 0 ? (float) frameCount / (float) sampleRate : 0.0f;
  return pcm;
}

static float* PCM_ConvertToStereo (float* src, int srcChannels, int frameCount) {
  if (srcChannels == PHX_AUDIO_CHANNELS)
    return src;

  float* stereo = MemNewArray(float, frameCount * PHX_AUDIO_CHANNELS);
  if (srcChannels == 1) {
    for (int i = frameCount - 1; i >= 0; --i) {
      stereo[i * 2 + 0] = src[i];
      stereo[i * 2 + 1] = src[i];
    }
  } else {
    for (int i = 0; i < frameCount; ++i) {
      stereo[i * 2 + 0] = src[i * srcChannels + 0];
      stereo[i * 2 + 1] = src[i * srcChannels + 1];
    }
  }
  MemFree(src);
  return stereo;
}

static PCMBuffer* LoadPCM_WAV (cstr path, bool* outOk) {
  SDL_AudioSpec spec;
  Uint8* raw = 0;
  Uint32 rawLen = 0;
  if (!SDL_LoadWAV(path, &spec, &raw, &rawLen))
    return 0;

  SDL_AudioSpec f32Spec = spec;
  f32Spec.format = SDL_AUDIO_F32;

  Uint8* f32Data = 0;
  int f32Len = 0;
  if (!SDL_ConvertAudioSamples(&spec, raw, (int) rawLen, &f32Spec, &f32Data, &f32Len)) {
    SDL_free(raw);
    return 0;
  }
  SDL_free(raw);

  int channels = spec.channels;
  int sampleRate = spec.freq;
  int frameCount = f32Len / (channels * (int) sizeof(float));
  float* samples = (float*) f32Data;

  if (channels != PHX_AUDIO_CHANNELS) {
    float* owned = MemNewArray(float, frameCount * channels);
    MemCpy(owned, samples, (size_t) f32Len);
    SDL_free(f32Data);
    samples = PCM_ConvertToStereo(owned, channels, frameCount);
  } else {
    float* owned = MemNewArray(float, frameCount * PHX_AUDIO_CHANNELS);
    MemCpy(owned, samples, (size_t) f32Len);
    SDL_free(f32Data);
    samples = owned;
  }

  *outOk = true;
  return PCMBuffer_Create(samples, frameCount, PHX_AUDIO_CHANNELS, sampleRate);
}

static PCMBuffer* LoadPCM_MP3 (cstr path, bool* outOk) {
  mp3dec_t dec;
  mp3dec_init(&dec);

  mp3dec_file_info_t info;
  MemZero(&info, sizeof(info));
  if (mp3dec_load(&dec, path, &info, 0, 0) != 0)
    return 0;

  int channels = info.channels;
  int sampleRate = info.hz;
  if (channels <= 0 || sampleRate <= 0 || !info.buffer || info.samples == 0) {
    free(info.buffer);
    return 0;
  }

  int frameCount = (int) (info.samples / (size_t) channels);
  float* samples = info.buffer;

  if (channels != PHX_AUDIO_CHANNELS)
    samples = PCM_ConvertToStereo(samples, channels, frameCount);
  else {
    float* owned = MemNewArray(float, frameCount * PHX_AUDIO_CHANNELS);
    MemCpy(owned, samples, (size_t) info.samples * sizeof(float));
    free(info.buffer);
    samples = owned;
  }

  *outOk = true;
  return PCMBuffer_Create(samples, frameCount, PHX_AUDIO_CHANNELS, sampleRate);
}

PCMBuffer* AudioBackend_LoadPCM (cstr path, bool* outOk) {
  *outOk = false;

  if (PathHasExt(path, ".wav") || PathHasExt(path, ".WAV"))
    return LoadPCM_WAV(path, outOk);

  if (PathHasExt(path, ".mp3") || PathHasExt(path, ".MP3"))
    return LoadPCM_MP3(path, outOk);

  Warn("AudioBackend_LoadPCM: Unsupported format.\n  Path: %s", path);
  return 0;
}

void AudioBackend_FreePCM (PCMBuffer* pcm) {
  if (!pcm) return;
  MemFree(pcm->samples);
  MemFree(pcm);
}

void AudioBackend_LoadPCM_Async (cstr path, SoundDesc* desc) {
  AsyncLoadJob* job = MemNew(AsyncLoadJob);
  job->desc = desc;
  job->path = StrDup(path);
  desc->loadStarted = true;
  SDL_Thread* thread = SDL_CreateThread(AsyncLoadThread, "AudioLoad", job);
  if (!thread) {
    desc->loadFailed = true;
    desc->loadComplete = false;
    StrFree(job->path);
    MemFree(job);
  }
}

static Voice* Voice_Alloc () {
  for (int i = 0; i < PHX_MAX_VOICES; ++i) {
    if (!self.voices[i].active) {
      MemZero(&self.voices[i], sizeof(Voice));
      self.voices[i].active = true;
      self.voices[i].volume = 1.0f;
      self.voices[i].pitch = 1.0f;
      self.voices[i].level3D = 1.0f;
      return &self.voices[i];
    }
  }
  return 0;
}

AudioVoiceId AudioBackend_CreateVoice (SoundDesc* desc, bool isLooped, bool is3D) {
  Voice* voice = Voice_Alloc();
  if (!voice) {
    Warn("AudioBackend_CreateVoice: voice pool exhausted");
    return AudioVoiceId_Invalid;
  }
  voice->pcm = desc->pcm;
  voice->looped = isLooped;
  voice->is3D = is3D;
  voice->paused = true;
  return (AudioVoiceId) (voice - self.voices);
}

void AudioBackend_DestroyVoice (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  MemZero(&self.voices[id], sizeof(Voice));
}

bool AudioBackend_IsVoicePlaying (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return false;
  Voice* v = &self.voices[id];
  return v->active && !v->paused && !v->finished;
}

void AudioBackend_VoicePlay (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].paused = false;
  self.voices[id].finished = false;
}

void AudioBackend_VoicePause (AudioVoiceId id, bool paused) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].paused = paused;
}

void AudioBackend_VoiceStop (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].paused = true;
  self.voices[id].finished = true;
}

void AudioBackend_VoiceRewind (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].playFrame = 0.0;
  self.voices[id].finished = false;
}

void AudioBackend_VoiceSetVolume (AudioVoiceId id, float volume) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].volume = volume;
}

void AudioBackend_VoiceSetPitch (AudioVoiceId id, float pitch) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].pitch = pitch;
}

void AudioBackend_VoiceSetPan (AudioVoiceId id, float pan) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].pan = pan;
}

void AudioBackend_VoiceSet3DLevel (AudioVoiceId id, float level) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  self.voices[id].level3D = level;
}

void AudioBackend_VoiceSet3DPos (AudioVoiceId id, Vec3f const* pos, Vec3f const* vel) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  if (pos) self.voices[id].pos = *pos;
  if (vel) self.voices[id].vel = *vel;
}

void AudioBackend_VoiceSetPlayPos (AudioVoiceId id, float seconds) {
  if (id < 0 || id >= PHX_MAX_VOICES) return;
  Voice* v = &self.voices[id];
  if (v->pcm && v->pcm->sampleRate > 0)
    v->playFrame = seconds * v->pcm->sampleRate;
}

bool AudioBackend_VoiceGetLooped (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return false;
  return self.voices[id].looped;
}

bool AudioBackend_VoiceGet3D (AudioVoiceId id) {
  if (id < 0 || id >= PHX_MAX_VOICES) return false;
  return self.voices[id].is3D;
}
