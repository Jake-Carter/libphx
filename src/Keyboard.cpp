#include "Keyboard.h"
#include "PhxMemory.h"
#include "SDL.h"

static uint64 lastAction;
static bool* stateLast;
static bool* stateCurr;
static int stateSize;

void Keyboard_Init () {
  int size = 0;
  bool const* state = SDL_GetKeyboardState(&size);
  stateSize = size;
  stateLast = MemNewArray(bool, size);
  stateCurr = MemNewArray(bool, size);
  MemCpy(stateLast, state, (size_t) size * sizeof(bool));
  MemCpy(stateCurr, state, (size_t) size * sizeof(bool));
  lastAction = SDL_GetPerformanceCounter();
}

void Keyboard_Free () {
  MemFree(stateLast);
  MemFree(stateCurr);
}

void Keyboard_UpdatePre () {
  int size = 0;
  bool const* state = SDL_GetKeyboardState(&size);
  if (size > stateSize) {
    stateLast = (bool*) MemRealloc(stateLast, (size_t) size * sizeof(bool));
    stateCurr = (bool*) MemRealloc(stateCurr, (size_t) size * sizeof(bool));
    stateSize = size;
  }
  MemCpy(stateLast, state, (size_t) size * sizeof(bool));
}

void Keyboard_UpdatePost () {
  int size = 0;
  bool const* state = SDL_GetKeyboardState(&size);
  if (size > stateSize) {
    stateLast = (bool*) MemRealloc(stateLast, (size_t) size * sizeof(bool));
    stateCurr = (bool*) MemRealloc(stateCurr, (size_t) size * sizeof(bool));
    stateSize = size;
  }
  MemCpy(stateCurr, state, (size_t) size * sizeof(bool));

  for (int i = 0; i < size; ++i) {
    if (stateCurr[i] != stateLast[i]) {
      lastAction = SDL_GetPerformanceCounter();
      break;
    }
  }
}

bool Keyboard_Down (Key key) {
   return stateCurr[key];
}

bool Keyboard_Pressed (Key key) {
  return stateCurr[key] && !stateLast[key];
}

bool Keyboard_Released (Key key) {
  return !stateCurr[key] && stateLast[key];
}

double Keyboard_GetIdleTime () {
  uint64 now = SDL_GetPerformanceCounter();
  return (double)(now - lastAction) / (double)SDL_GetPerformanceFrequency();
}

bool KeyMod_Alt () {
  return stateCurr[SDL_SCANCODE_LALT] || stateCurr[SDL_SCANCODE_RALT];
}

bool KeyMod_Ctrl () {
  return stateCurr[SDL_SCANCODE_LCTRL] || stateCurr[SDL_SCANCODE_RCTRL];
}

bool KeyMod_Shift () {
  return stateCurr[SDL_SCANCODE_LSHIFT] || stateCurr[SDL_SCANCODE_RSHIFT];
}
