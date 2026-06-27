#include "Audio.h"
#include "AudioBackend.h"
#include "PhxMemory.h"
#include "PhxMath.h"
#include "PhxString.h"
#include "Sound.h"
#include "SoundDesc.h"
#include "SoundDef.h"
#include "Vec3.h"

static void Sound_SetState (Sound*, SoundState);

static inline bool PathHasExt (cstr path, cstr ext) {
  cstr dot = StrFind(path, ext);
  return dot && StrEqual(dot, ext);
}

static void Sound_EnsureLoadedImpl (Sound* self, cstr func) {
  if (self->state != SoundState_Loading) return;

  SoundDesc_FinishLoad(self->desc, func);
  self->voice = AudioBackend_CreateVoice(
    self->desc,
    SoundDesc_GetLooped(self->desc),
    SoundDesc_Get3D(self->desc));

  if (self->voice == AudioVoiceId_Invalid)
    Fatal("%s: Failed to create audio voice.\n  Name: %s", func, self->desc->name);

  AudioBackend_VoicePause(self->voice, true);

  if (SoundDesc_Get3D(self->desc)) {
    Vec3f zero = { 0, 0, 0 };
    Sound_Set3DPos(self, &zero, &zero);
  }

  Sound_SetState(self, SoundState_Paused);
}
#define Sound_EnsureLoaded(...) Sound_EnsureLoadedImpl(__VA_ARGS__, __func__)

inline static void Sound_EnsureNotFreedImpl (Sound* self, cstr func) {
  if (self->state == SoundState_Freed) {
    cstr name = (self->desc->_refCount > 0) ? self->desc->name : "<SoundDesc has been freed>";
    Fatal("%s: Sound has been freed.\n  Name: %s", func, name);
  }
}
#define Sound_EnsureNotFreed(...) Sound_EnsureNotFreedImpl(__VA_ARGS__, __func__)

inline static void Sound_EnsureStateImpl (Sound* self, cstr func) {
  Sound_EnsureLoadedImpl(self, func);
  Sound_EnsureNotFreedImpl(self, func);
}
#define Sound_EnsureState(...) Sound_EnsureStateImpl(__VA_ARGS__, __func__)

static void Sound_SetState (Sound* self, SoundState nextState) {
  if (nextState == self->state) return;

  switch (nextState) {
    default: Fatal("Sound_SetState: Unhandled case: %i", nextState);

    case SoundState_Loading:
      Assert(self->state == SoundState_Null);
      break;

    case SoundState_Playing:
      AudioBackend_VoicePlay(self->voice);
      break;

    case SoundState_Paused:
      AudioBackend_VoicePause(self->voice, true);
      break;

    case SoundState_Finished:
      AudioBackend_VoiceStop(self->voice);
      break;

    case SoundState_Freed:
      break;
  }

  self->state = nextState;
  Audio_SoundStateChanged(self);

  if (self->freeOnFinish && self->state == SoundState_Finished)
    Sound_Free(self);
}

static Sound* Sound_Create (cstr name, bool immediate, bool isLooped, bool is3D) {
  SoundDesc* desc = SoundDesc_Load(name, immediate, isLooped, is3D);
  Sound* self = Audio_AllocSound();
  self->desc = desc;
  self->voice = AudioVoiceId_Invalid;
  Sound_SetState(self, SoundState_Loading);
  return self;
}

Sound* Sound_Load (cstr name, bool isLooped, bool is3D) {
  Sound* self = Sound_Create(name, true, isLooped, is3D);
  Sound_EnsureLoaded(self);
  return self;
}

Sound* Sound_LoadAsync (cstr name, bool isLooped, bool is3D) {
  return Sound_Create(name, false, isLooped, is3D);
}

Sound* Sound_Clone (Sound* self) {
  Sound_EnsureState(self);
  Sound* clone = Audio_AllocSound();
  *clone = *self;
  SoundDesc_Acquire(self->desc);
  clone->voice = AudioVoiceId_Invalid;
  clone->state = SoundState_Null;
  Sound_SetState(clone, SoundState_Loading);
  return clone;
}

void Sound_ToFile (Sound* self, cstr name) {
  Sound_EnsureState(self);
  SoundDesc_ToFile(self->desc, name);
}

void Sound_Acquire (Sound* self) {
  Sound_EnsureState(self);
  RefCounted_Acquire(self->desc);
}

void Sound_Free (Sound* self) {
  Sound_EnsureState(self);
  Sound_SetState(self, SoundState_Finished);
  if (self->voice != AudioVoiceId_Invalid)
    AudioBackend_DestroyVoice(self->voice);
  Sound_SetState(self, SoundState_Freed);
  SoundDesc_Free(self->desc);
}

void Sound_Play (Sound* self) {
  Sound_EnsureState(self);
  Sound_SetState(self, SoundState_Playing);
}

void Sound_Pause (Sound* self) {
  Sound_EnsureState(self);
  Sound_SetState(self, SoundState_Paused);
}

void Sound_Rewind (Sound* self) {
  Sound_EnsureState(self);
  AudioBackend_VoiceRewind(self->voice);
}

bool Sound_Get3D (Sound* self) {
  Sound_EnsureState(self);
  return AudioBackend_VoiceGet3D(self->voice);
}

float Sound_GetDuration (Sound* self) {
  Sound_EnsureState(self);
  return SoundDesc_GetDuration(self->desc);
}

bool Sound_GetLooped (Sound* self) {
  Sound_EnsureState(self);
  return AudioBackend_VoiceGetLooped(self->voice);
}

cstr Sound_GetName (Sound* self) {
  Sound_EnsureNotFreed(self);
  return SoundDesc_GetName(self->desc);
}

cstr Sound_GetPath (Sound* self) {
  Sound_EnsureNotFreed(self);
  return SoundDesc_GetPath(self->desc);
}

bool Sound_IsFinished (Sound* self) {
  return self->state == SoundState_Finished;
}

bool Sound_IsPlaying (Sound* self) {
  return self->state == SoundState_Playing;
}

void Sound_Attach3DPos (Sound* self, Vec3f const* pos, Vec3f const* vel) {
  Sound_Set3DPos(self, pos, vel);
  self->autoPos = pos;
  self->autoVel = vel;
}

void Sound_Set3DLevel (Sound* self, float level) {
  Sound_EnsureState(self);
  AudioBackend_VoiceSet3DLevel(self->voice, level);
}

void Sound_Set3DPos (Sound* self, Vec3f const* pos, Vec3f const* vel) {
  Sound_EnsureState(self);
  AudioBackend_VoiceSet3DPos(self->voice, pos, vel);
}

void Sound_SetFreeOnFinish (Sound* self, bool freeOnFinish) {
  self->freeOnFinish = freeOnFinish;
}

void Sound_SetPan (Sound* self, float pan) {
  Sound_EnsureState(self);
  AudioBackend_VoiceSetPan(self->voice, pan);
}

void Sound_SetPitch (Sound* self, float pitch) {
  Sound_EnsureState(self);
  AudioBackend_VoiceSetPitch(self->voice, pitch);
}

void Sound_SetPlayPos (Sound* self, float seconds) {
  Sound_EnsureState(self);
  Assert(seconds >= 0.0f);
  AudioBackend_VoiceSetPlayPos(self->voice, seconds);
}

void Sound_SetVolume (Sound* self, float volume) {
  Sound_EnsureState(self);
  AudioBackend_VoiceSetVolume(self->voice, volume);
}

Sound* Sound_LoadPlay (cstr name, bool isLooped, bool is3D) {
  Sound* self = Sound_Load(name, isLooped, is3D);
  Sound_Play(self);
  return self;
}

Sound* Sound_LoadPlayAttached (cstr name, bool isLooped, bool is3D, Vec3f const* pos, Vec3f const* vel) {
  Sound* self = Sound_Load(name, isLooped, is3D);
  Sound_Attach3DPos(self, pos, vel);
  Sound_Play(self);
  return self;
}

void Sound_LoadPlayFree (cstr name, bool isLooped, bool is3D) {
  Sound* self = Sound_Load(name, isLooped, is3D);
  Sound_SetFreeOnFinish(self, true);
  Sound_Play(self);
}

void Sound_LoadPlayFreeAttached (cstr name, bool isLooped, bool is3D, Vec3f const* pos, Vec3f const* vel) {
  Sound* self = Sound_Load(name, isLooped, is3D);
  Sound_Attach3DPos(self, pos, vel);
  Sound_SetFreeOnFinish(self, true);
  Sound_Play(self);
}

Sound* Sound_ClonePlay (Sound* self) {
  Sound* clone = Sound_Clone(self);
  Sound_Play(clone);
  return clone;
}

Sound* Sound_ClonePlayAttached (Sound* self, Vec3f const* pos, Vec3f const* vel) {
  Sound* clone = Sound_Clone(self);
  Sound_Attach3DPos(clone, pos, vel);
  Sound_Play(clone);
  return clone;
}

void Sound_ClonePlayFree (Sound* self) {
  Sound* clone = Sound_Clone(self);
  Sound_SetFreeOnFinish(clone, true);
  Sound_Play(clone);
}

void Sound_ClonePlayFreeAttached (Sound* self, Vec3f const* pos, Vec3f const* vel) {
  Sound* clone = Sound_Clone(self);
  Sound_Attach3DPos(clone, pos, vel);
  Sound_SetFreeOnFinish(clone, true);
  Sound_Play(clone);
}

void Sound_Update (Sound* self) {
  if (self->state == SoundState_Loading) return;

  if (Sound_Get3D(self))
    Sound_Set3DPos(self, self->autoPos, self->autoVel);

  if (self->state == SoundState_Playing && self->voice != AudioVoiceId_Invalid) {
    if (!AudioBackend_IsVoicePlaying(self->voice))
      Sound_SetState(self, SoundState_Finished);
  }
}

bool Sound_IsFreed (Sound* self) {
  return self->state == SoundState_Freed;
}
