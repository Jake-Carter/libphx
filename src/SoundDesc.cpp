#include "Audio.h"
#include "AudioBackend.h"
#include "File.h"
#include "PhxMemory.h"
#include "Resource.h"
#include "Sound.h"
#include "SoundDesc.h"
#include "SoundDef.h"
#include "PhxString.h"

#include <SDL3/SDL.h>

struct SoundDescLoadInfo {
  bool isLooped;
  bool is3D;
};

static bool SoundDesc_LoadInfoFromKey (cstr mapKey, SoundDescLoadInfo* info) {
  if (StrBegins(mapKey, "LOOPED:")) {
    info->isLooped = true;
    return true;
  }
  if (StrBegins(mapKey, "UNLOOPED:")) {
    info->isLooped = false;
    return true;
  }
  return false;
}

void SoundDesc_FinishLoad (SoundDesc* self, cstr func) {
  if (self->loadComplete) return;

  if (self->loadFailed)
    Fatal("%s: Background file load has failed.\n  Path: %s", func, self->path);

  if (self->loadStarted && !self->loadComplete) {
    Warn("%s: Background file load hasn't finished. Blocking the main thread.\n  Path: %s", func, self->path);
    while (!self->loadComplete && !self->loadFailed)
      SDL_Delay(1);
  }

  if (self->loadFailed || !self->pcm)
    Fatal("%s: Failed to load sound.\n  Path: %s", func, self->path);
}

SoundDesc* SoundDesc_Load (cstr name, bool immediate, bool isLooped, bool is3D) {
  cstr mapKey = StrAdd(isLooped ? "LOOPED:" : "UNLOOPED:", name);
  SoundDesc* self = Audio_AllocSoundDesc(mapKey);
  StrFree(mapKey);

  if (!self->name) {
    cstr path = Resource_GetPath(ResourceType_Sound, name);

    self->name = StrDup(name);
    self->path = StrDup(path);
    self->isLooped = isLooped;
    self->is3D = is3D;
    RefCounted_Init(self);

    if (immediate) {
      bool ok = false;
      self->pcm = AudioBackend_LoadPCM(path, &ok);
      self->loadComplete = ok;
      self->loadFailed = !ok;
      self->loadStarted = true;
    } else {
      AudioBackend_LoadPCM_Async(path, self);
    }
  } else {
    RefCounted_Acquire(self);
    if (immediate)
      SoundDesc_FinishLoad(self, __func__);
  }

  return self;
}

void SoundDesc_Acquire (SoundDesc* desc) {
  RefCounted_Acquire(desc);
}

void SoundDesc_Free (SoundDesc* desc) {
  RefCounted_Free(desc) {
    cstr name = desc->name;
    cstr path = desc->path;
    if (desc->pcm)
      AudioBackend_FreePCM(desc->pcm);
    Audio_DeallocSoundDesc(desc);
    StrFree(name);
    StrFree(path);
    MemZero(desc, sizeof(SoundDesc));
  }
}

float SoundDesc_GetDuration (SoundDesc* desc) {
  SoundDesc_FinishLoad(desc, __func__);
  return desc->pcm ? desc->pcm->duration : 0.0f;
}

cstr SoundDesc_GetName (SoundDesc* desc) {
  return desc->name;
}

cstr SoundDesc_GetPath (SoundDesc* desc) {
  return desc->path;
}

bool SoundDesc_GetLooped (SoundDesc* desc) {
  return desc->isLooped;
}

bool SoundDesc_Get3D (SoundDesc* desc) {
  return desc->is3D;
}

void SoundDesc_ToFile (SoundDesc* desc, cstr name) {
  SoundDesc_FinishLoad(desc, __func__);
  if (!desc->pcm) return;

  File* file = File_Create(name);
  if (!file)
    Fatal("SoundDesc_ToFile: Failed to create file.\nPath: %s", name);

  int channels = desc->pcm->channels;
  int sampleRate = desc->pcm->sampleRate;
  int frameCount = desc->pcm->frameCount;
  int dataBytes = frameCount * channels * (int) sizeof(int16);

  File_Write   (file, "RIFF", 4);
  File_WriteI32(file, 36 + dataBytes);
  File_Write   (file, "WAVE", 4);
  File_Write   (file, "fmt ", 4);
  File_WriteI32(file, 16);
  File_WriteI16(file, 1);
  File_WriteI16(file, (int16) channels);
  File_WriteI32(file, (int32) sampleRate);
  File_WriteI32(file, (int32) (channels * sampleRate * sizeof(int16)));
  File_WriteI16(file, (int16) (channels * (int) sizeof(int16)));
  File_WriteI16(file, 16);
  File_Write   (file, "data", 4);
  File_WriteI32(file, dataBytes);

  for (int i = 0; i < frameCount * channels; ++i) {
    float sample = desc->pcm->samples[i];
    sample = Clamp(sample, -1.0f, 1.0f);
    int16 pcm = (int16) (sample * 32767.0f);
    File_WriteI16(file, pcm);
  }

  File_Close(file);
}
