#pragma once

#include <EGL/egl.h>
#include <GLES3/gl32.h>

GLuint gl_compile_program(const char *vs_src, const char *fs_src,
                          const char *label);

void gl_check(const char *where);

bool gl_make_current(EGLDisplay display, EGLSurface surface,
                     EGLContext context);
