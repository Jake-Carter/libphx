#ifndef PHX_AudioBackend
#define PHX_AudioBackend

#include "Common.h"
#include "Vec3.h"

struct SoundDesc;
struct Sound;

/* PCM sample data decoded from wav/mp3 */
struct PCMBuffer {
  float*  samples;     /* interleaved float32, stereo */
  int     frameCount;  /* frames (pairs of samples for stereo) */
  int     channels;
  int     sampleRate;
  float   duration;    /* seconds */
};

/* Opaque voice handle managed by the backend */
typedef int AudioVoiceId;
const AudioVoiceId AudioVoiceId_Invalid = -1;

void        AudioBackend_Init               ();
void        AudioBackend_Free               ();
void        AudioBackend_Update               ();

void        AudioBackend_Set3DSettings        (float doppler, float scale, float rolloff);
void        AudioBackend_SetListenerPos       (Vec3f const* pos, Vec3f const* vel, Vec3f const* fwd, Vec3f const* up);

/* SoundDesc lifecycle */
PCMBuffer*  AudioBackend_LoadPCM              (cstr path, bool* outOk);
void        AudioBackend_FreePCM              (PCMBuffer* pcm);
void        AudioBackend_LoadPCM_Async        (cstr path, SoundDesc* desc);

/* Voice lifecycle */
AudioVoiceId AudioBackend_CreateVoice         (SoundDesc* desc, bool isLooped, bool is3D);
void         AudioBackend_DestroyVoice        (AudioVoiceId voice);
bool         AudioBackend_IsVoicePlaying      (AudioVoiceId voice);

void         AudioBackend_VoicePlay           (AudioVoiceId voice);
void         AudioBackend_VoicePause          (AudioVoiceId voice, bool paused);
void         AudioBackend_VoiceStop           (AudioVoiceId voice);
void         AudioBackend_VoiceRewind         (AudioVoiceId voice);

void         AudioBackend_VoiceSetVolume      (AudioVoiceId voice, float volume);
void         AudioBackend_VoiceSetPitch       (AudioVoiceId voice, float pitch);
void         AudioBackend_VoiceSetPan         (AudioVoiceId voice, float pan);
void         AudioBackend_VoiceSet3DLevel     (AudioVoiceId voice, float level);
void         AudioBackend_VoiceSet3DPos       (AudioVoiceId voice, Vec3f const* pos, Vec3f const* vel);
void         AudioBackend_VoiceSetPlayPos     (AudioVoiceId voice, float seconds);

bool         AudioBackend_VoiceGetLooped      (AudioVoiceId voice);
bool         AudioBackend_VoiceGet3D          (AudioVoiceId voice);

#endif
