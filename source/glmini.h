// Minimal self-contained OpenGL 3.3 core loader (works with Mesa/EGL on Switch and SDL on PC)
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef void GLvoid;

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_LINES 0x0001
#define GL_LEQUAL 0x0203
#define GL_LESS 0x0201
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ZERO 0
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RED 0x1903
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_R8 0x8229
#define GL_RGBA8 0x8058
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#define GL_MULTISAMPLE 0x809D
#define GL_FRAMEBUFFER_SRGB 0x8DB9

#define GL_FUNCS(X) \
  X(void, glClear, (GLbitfield m)) \
  X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
  X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h)) \
  X(void, glScissor, (GLint x, GLint y, GLsizei w, GLsizei h)) \
  X(void, glEnable, (GLenum c)) \
  X(void, glDisable, (GLenum c)) \
  X(void, glBlendFunc, (GLenum s, GLenum d)) \
  X(void, glBlendFuncSeparate, (GLenum a, GLenum b, GLenum c, GLenum d)) \
  X(void, glDepthMask, (GLboolean f)) \
  X(void, glDepthFunc, (GLenum f)) \
  X(void, glCullFace, (GLenum m)) \
  X(void, glFrontFace, (GLenum m)) \
  X(void, glPolygonOffset, (GLfloat f, GLfloat u)) \
  X(GLenum, glGetError, (void)) \
  X(void, glPixelStorei, (GLenum p, GLint v)) \
  X(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void* d)) \
  X(void, glGenBuffers, (GLsizei n, GLuint* b)) \
  X(void, glBindBuffer, (GLenum t, GLuint b)) \
  X(void, glBufferData, (GLenum t, GLsizeiptr s, const void* d, GLenum u)) \
  X(void, glBufferSubData, (GLenum t, GLintptr o, GLsizeiptr s, const void* d)) \
  X(void, glGenVertexArrays, (GLsizei n, GLuint* a)) \
  X(void, glBindVertexArray, (GLuint a)) \
  X(void, glVertexAttribPointer, (GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p)) \
  X(void, glEnableVertexAttribArray, (GLuint i)) \
  X(void, glDrawArrays, (GLenum m, GLint f, GLsizei c)) \
  X(void, glDrawElements, (GLenum m, GLsizei c, GLenum t, const void* i)) \
  X(GLuint, glCreateShader, (GLenum t)) \
  X(void, glShaderSource, (GLuint s, GLsizei c, const GLchar* const* str, const GLint* l)) \
  X(void, glCompileShader, (GLuint s)) \
  X(void, glGetShaderiv, (GLuint s, GLenum p, GLint* v)) \
  X(void, glGetShaderInfoLog, (GLuint s, GLsizei m, GLsizei* l, GLchar* i)) \
  X(GLuint, glCreateProgram, (void)) \
  X(void, glAttachShader, (GLuint p, GLuint s)) \
  X(void, glBindAttribLocation, (GLuint p, GLuint i, const GLchar* n)) \
  X(void, glLinkProgram, (GLuint p)) \
  X(void, glGetProgramiv, (GLuint p, GLenum e, GLint* v)) \
  X(void, glGetProgramInfoLog, (GLuint p, GLsizei m, GLsizei* l, GLchar* i)) \
  X(void, glDeleteShader, (GLuint s)) \
  X(void, glUseProgram, (GLuint p)) \
  X(GLint, glGetUniformLocation, (GLuint p, const GLchar* n)) \
  X(void, glUniform1i, (GLint l, GLint v)) \
  X(void, glUniform1f, (GLint l, GLfloat v)) \
  X(void, glUniform2f, (GLint l, GLfloat a, GLfloat b)) \
  X(void, glUniform3f, (GLint l, GLfloat a, GLfloat b, GLfloat c)) \
  X(void, glUniform4f, (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d)) \
  X(void, glUniformMatrix4fv, (GLint l, GLsizei c, GLboolean t, const GLfloat* v)) \
  X(void, glGenTextures, (GLsizei n, GLuint* t)) \
  X(void, glBindTexture, (GLenum t, GLuint x)) \
  X(void, glActiveTexture, (GLenum t)) \
  X(void, glTexImage2D, (GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* d)) \
  X(void, glTexParameteri, (GLenum t, GLenum p, GLint v)) \
  X(void, glTexParameteriv, (GLenum t, GLenum p, const GLint* v)) \
  X(void, glGenerateMipmap, (GLenum t))

// pointers are named gm_<name> to avoid clashing with symbols exported by system GL libraries
#define GL_DECL(ret, name, args) typedef ret (GLAPIENTRY *PFN_##name) args; extern PFN_##name gm_##name;
GL_FUNCS(GL_DECL)
#undef GL_DECL
#define GL_ALIAS(ret, name, args)
#include "glmini_alias.h"

typedef void* (*GLGetProcFn)(const char*);
bool glminiLoad(GLGetProcFn fn);
