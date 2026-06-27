#ifndef PHX_OpenGL
#define PHX_OpenGL

#include "Common.h"
#include <glad/gl.h>

void  OpenGL_Init       ();
void  OpenGL_CheckError (cstr file, int line);

#if ENABLE_GLCHECK
  #define GLCALL(x) { x; OpenGL_CheckError(__FILE__, __LINE__); }
#else
  #define GLCALL(x) x;
#endif

#endif
