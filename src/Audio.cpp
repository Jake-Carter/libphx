#include "ArrayList.h"
#include "Audio.h"
#include "AudioBackend.h"
#include "MemPool.h"
#include "PhxMath.h"
#include "Sound.h"
#include "SoundDef.h"
#include "StrMap.h"
#include "Vec3.h"

#define AUDIO_CHANNELS 1024
#define SOUNDPOOL_BLOCK_SIZE 128

struct Audio {
  StrMap*           descMap;
  MemPool*          soundPool;
  ArrayList(Sound*, playingSounds);
  ArrayList(Sound*, freeingSounds);

  Vec3f const* autoPos;
  Vec3f const* autoVel;
  Vec3f const* autoFwd;
  Vec3f const* autoUp;
} static self;

void Audio_Init () {
  AudioBackend_Init();
  self.descMap = StrMap_Create(128);
  self.soundPool = MemPool_Create(sizeof(Sound), SOUNDPOOL_BLOCK_SIZE);
}

void Audio_Free () {
  AudioBackend_Free();
  StrMap_Free(self.descMap);
  MemPool_Free(self.soundPool);
  ArrayList_Free(self.playingSounds);
  ArrayList_Free(self.freeingSounds);
}

void Audio_AttachListenerPos (Vec3f const* pos, Vec3f const* vel, Vec3f const* fwd, Vec3f const* up) {
  self.autoPos = pos;
  self.autoVel = vel;
  self.autoFwd = fwd;
  self.autoUp  = up;
  Audio_SetListenerPos(pos, vel, fwd, up);
}

void Audio_Set3DSettings (float doppler, float scale, float rolloff) {
  AudioBackend_Set3DSettings(doppler, scale, rolloff);
}

void Audio_SetListenerPos (
  Vec3f const* pos,
  Vec3f const* vel,
  Vec3f const* fwd,
  Vec3f const* up)
{
  Assert(!fwd || Approx(Vec3f_Length(*fwd), 1));
  Assert(!up || Approx(Vec3f_Length(*up), 1));
  Assert(!fwd || !up || Approx(Vec3f_Dot(*fwd, *up), 0));
  AudioBackend_SetListenerPos(pos, vel, fwd, up);
}

void Audio_Update () {
  AudioBackend_Update();
  Audio_SetListenerPos(self.autoPos, self.autoVel, self.autoFwd, self.autoUp);

  ArrayList_ForEachI(self.playingSounds, i) {
    Sound* sound = ArrayList_Get(self.playingSounds, i);
    if (!Sound_IsFreed(sound) && Sound_IsPlaying(sound)) {
      Sound_Update(sound);
      if (!Sound_IsPlaying(sound))
        ArrayList_RemoveAtFast(self.playingSounds, i--);
    } else {
      ArrayList_RemoveAtFast(self.playingSounds, i--);
    }
  }

  ArrayList_ForEachI(self.freeingSounds, i) {
    Sound* sound = ArrayList_Get(self.freeingSounds, i);
    Audio_DeallocSound(sound);
  }
  ArrayList_Clear(self.freeingSounds);
}

int32 Audio_GetLoadedCount () {
  uint32 size = StrMap_GetSize(self.descMap);
  Assert(size <= INT32_MAX);
  return (int32) size;
}

int32 Audio_GetPlayingCount () {
  return ArrayList_GetSize (self.playingSounds);
}

int32 Audio_GetTotalCount () {
  uint32 size = MemPool_GetSize(self.soundPool);
  Assert(size <= INT32_MAX);
  return (int32) size;
}

void* Audio_GetHandle () {
  return 0;
}

SoundDesc* Audio_AllocSoundDesc (cstr name) {
  SoundDesc* desc = (SoundDesc*) StrMap_Get(self.descMap, name);
  if (!desc) {
    desc = MemNewZero(SoundDesc);
    StrMap_Set(self.descMap, name, desc);
  }
  return desc;
}

void Audio_DeallocSoundDesc (SoundDesc* desc) {
  StrMap_Remove(self.descMap, desc->name);
  MemFree(desc);
}

Sound* Audio_AllocSound () {
  return (Sound*) MemPool_Alloc(self.soundPool);
}

void Audio_DeallocSound (Sound* sound) {
  MemPool_Dealloc(self.soundPool, sound);
}

void Audio_SoundStateChanged (Sound* sound) {
  if (Sound_IsFreed(sound)) {
    ArrayList_Append(self.freeingSounds, sound);
  } else if (Sound_IsPlaying(sound)) {
    ArrayList_Append(self.playingSounds, sound);

    CHECK1(
      if (ArrayList_GetSize(self.playingSounds) == AUDIO_CHANNELS + 1)
        Warn("Audio: Exceeded the number of available sound channels (%i)", AUDIO_CHANNELS);
    )
  }
}
