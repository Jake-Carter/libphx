#ifndef PHX_OpenGL
#define PHX_OpenGL

#include "Common.h"
#include <glad/gl.h>

#ifndef GL_RGBA16F
#define GL_RGBA16F GL_RGBA16F_ARB
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F GL_RGBA32F_ARB
#endif

void  OpenGL_Init       ();
void  OpenGL_CheckError (cstr file, int line);

#if ENABLE_GLCHECK
  #define GLCALL(x) { x; OpenGL_CheckError(__FILE__, __LINE__); }
#else
  #define GLCALL(x) x;
#endif

#endif
