
/*
File: nuklear_sdl2_gl2_implementation.c
Date: Mon 24 Feb 2025 03:00:23 PM EET

Compile this file with -c and run "ar rcs libnuklear_sdl2_gl2.a <resulting object>"
to generate static library.
Link against the resulting static library, do not define NK_IMPLEMENTATION in source files. 
Cuts down compile time in half at least!
*/

#include "SDL.h"
#include <GL/gl.h>
#include <GL/glu.h>

#define NK_INCLUDE_FIXED_TYPES              1
#define NK_INCLUDE_STANDARD_IO              1
#define NK_INCLUDE_STANDARD_VARARGS         1
#define NK_INCLUDE_DEFAULT_ALLOCATOR        1
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT     1
#define NK_INCLUDE_FONT_BAKING              1
#define NK_INCLUDE_DEFAULT_FONT             1
#define NK_IMPLEMENTATION                   1
#define NK_SDL_GL2_IMPLEMENTATION           1
#include "nuklear.h"
#include "nuklear_sdl_gl2.h"
