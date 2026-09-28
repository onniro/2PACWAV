
/*
File: 2pacwav2_glext.h
Date: Tue 22 Sep 2026 02:34:31 PM EEST

2pacwav opengl loader
*/

#ifndef _2PACWAV2_GLEXT_DOT_H

#if _2PACWAV_MINGW32
    #include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>
#include <SDL_opengl.h>

static PFNGLGENVERTEXARRAYSPROC            glextGenVertexArrays;
static PFNGLGENBUFFERSPROC                 glextGenBuffers;
static PFNGLBINDVERTEXARRAYPROC            glextBindVertexArray;
static PFNGLBINDBUFFERPROC                 glextBindBuffer;
static PFNGLBUFFERDATAPROC                 glextBufferData;
static PFNGLENABLEVERTEXATTRIBARRAYPROC    glextEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC        glextVertexAttribPointer;
static PFNGLCREATESHADERPROC               glextCreateShader;
static PFNGLSHADERSOURCEPROC               glextShaderSource;
static PFNGLCOMPILESHADERPROC              glextCompileShader;
static PFNGLGETSHADERIVPROC                glextGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC           glextGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC              glextCreateProgram;
static PFNGLATTACHSHADERPROC               glextAttachShader;
static PFNGLLINKPROGRAMPROC                glextLinkProgram;
static PFNGLVALIDATEPROGRAMPROC            glextValidateProgram;
static PFNGLDELETESHADERPROC               glextDeleteShader;
static PFNGLGETUNIFORMLOCATIONPROC         glextGetUniformLocation;
static PFNGLUSEPROGRAMPROC                 glextUseProgram;
static PFNGLUNIFORM4FPROC                  glextUniform4f;
static PFNGLBUFFERSUBDATAPROC              glextBufferSubData;

static bool load_used_gl_extensions(void)
{
#define LOAD_GLEXT_PROC(alias, actual_name) \
    alias = reinterpret_cast<decltype(alias)>(SDL_GL_GetProcAddress(#actual_name)); \
    if (!alias) { return false; }

    LOAD_GLEXT_PROC(glextGenVertexArrays, glGenVertexArrays);
    LOAD_GLEXT_PROC(glextGenBuffers, glGenBuffers);
    LOAD_GLEXT_PROC(glextBindVertexArray, glBindVertexArray);
    LOAD_GLEXT_PROC(glextBindBuffer, glBindBuffer);
    LOAD_GLEXT_PROC(glextBufferData, glBufferData);
    LOAD_GLEXT_PROC(glextEnableVertexAttribArray, glEnableVertexAttribArray);
    LOAD_GLEXT_PROC(glextVertexAttribPointer, glVertexAttribPointer);
    LOAD_GLEXT_PROC(glextCreateShader, glCreateShader);
    LOAD_GLEXT_PROC(glextShaderSource, glShaderSource);
    LOAD_GLEXT_PROC(glextCompileShader, glCompileShader);
    LOAD_GLEXT_PROC(glextGetShaderiv, glGetShaderiv);
    LOAD_GLEXT_PROC(glextGetShaderInfoLog, glGetShaderInfoLog);
    LOAD_GLEXT_PROC(glextCreateProgram, glCreateProgram);
    LOAD_GLEXT_PROC(glextAttachShader, glAttachShader);
    LOAD_GLEXT_PROC(glextLinkProgram, glLinkProgram);
    LOAD_GLEXT_PROC(glextValidateProgram, glValidateProgram);
    LOAD_GLEXT_PROC(glextDeleteShader, glDeleteShader);
    LOAD_GLEXT_PROC(glextGetUniformLocation, glGetUniformLocation);
    LOAD_GLEXT_PROC(glextUseProgram, glUseProgram);
    LOAD_GLEXT_PROC(glextUniform4f, glUniform4f);
    LOAD_GLEXT_PROC(glextBufferSubData, glBufferSubData);

    return true;
}

#define _2PACWAV2_GLEXT_DOT_H 1
#endif
