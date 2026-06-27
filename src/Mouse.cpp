#include "Mouse.h"
#include "SDL.h"
#include "Vec2.h"

static float lastX;
static float lastY;
static uint32 lastState;

static uint64 lastAction;
static int scrollAmount;

static uint32 Mouse_GetState (int* x, int* y) {
  float fx = 0, fy = 0;
  uint32 state = (uint32) SDL_GetMouseState(&fx, &fy);
  if (x) *x = (int) fx;
  if (y) *y = (int) fy;
  return state;
}

void Mouse_Init () {
  int x, y;
  lastState = Mouse_GetState(&x, &y);
  lastX = (float) x;
  lastY = (float) y;
  lastAction = SDL_GetPerformanceCounter();
  scrollAmount = 0;
}

void Mouse_Free () {
}

void Mouse_SetScroll (int amount) {
  scrollAmount = amount;
}

void Mouse_Update () {
  int x, y;
  int lx = (int) lastX, ly = (int) lastY;
  uint32 state = lastState;
  lastState = Mouse_GetState(&x, &y);
  lastX = (float) x;
  lastY = (float) y;
  if (lx != x || ly != y || state != lastState)
    lastAction = SDL_GetPerformanceCounter();
  scrollAmount = 0;
}

void Mouse_GetDelta (Vec2i* out) {
  int x, y;
  Mouse_GetState(&x, &y);
  out->x = x - (int) lastX;
  out->y = y - (int) lastY;
}

double Mouse_GetIdleTime () {
  uint64 now = SDL_GetPerformanceCounter();
  return (double)(now - lastAction) / (double)SDL_GetPerformanceFrequency();
}

void Mouse_GetPosition (Vec2i* out) {
  Mouse_GetState(&out->x, &out->y);
}

void Mouse_GetPositionGlobal (Vec2i* out) {
  float fx = 0, fy = 0;
  SDL_GetGlobalMouseState(&fx, &fy);
  out->x = (int) fx;
  out->y = (int) fy;
}

int Mouse_GetScroll () {
  return scrollAmount;
}

void Mouse_SetPosition (int x, int y) {
  SDL_WarpMouseInWindow(NULL, (float) x, (float) y);
}

void Mouse_SetVisible (bool visible) {
  if (visible) SDL_ShowCursor(); else SDL_HideCursor();
}

bool Mouse_Down (MouseButton button) {
  return (Mouse_GetState(0, 0) & SDL_BUTTON_MASK(button)) != 0;
}

bool Mouse_Pressed (MouseButton button) {
  uint32 current = Mouse_GetState(0, 0);
  uint32 mask = SDL_BUTTON_MASK(button);
  return (current & mask) && !(lastState & mask);
}

bool Mouse_Released (MouseButton button) {
  uint32 current = Mouse_GetState(0, 0);
  uint32 mask = SDL_BUTTON_MASK(button);
  return !(current & mask) && (lastState & mask);
}
