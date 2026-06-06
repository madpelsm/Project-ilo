#pragma once
// Single include point for the OpenGL API.
// Desktop builds use the generated glad loader; Emscripten/web builds use the
// WebGL2 (OpenGL ES 3.0) headers that ship with the toolchain.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
