#ifndef PHX_SoundDef
#define PHX_SoundDef

#include "AudioBackend.h"
#include "Common.h"
#include "RefCounted.h"

struct SoundDesc {
  RefCounted;
  PCMBuffer* pcm;
  cstr       name;
  cstr       path;
  bool       isLooped;
  bool       is3D;
  bool       loadStarted;
  bool       loadComplete;
  bool       loadFailed;
};

typedef uint8 SoundState;
const SoundState SoundState_Null     = 0;
const SoundState SoundState_Loading  = 1;
const SoundState SoundState_Paused   = 2;
const SoundState SoundState_Playing  = 3;
const SoundState SoundState_Finished = 4;
const SoundState SoundState_Freed    = 5;

struct Sound {
  SoundDesc*    desc;
  AudioVoiceId  voice;
  SoundState    state;
  Vec3f const*  autoPos;
  Vec3f const*  autoVel;
  bool          freeOnFinish;
};

#endif
