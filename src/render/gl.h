#pragma once

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <string>

GLuint gl_compile_program(const char *vs_src, const char *fs_src,
                          const char *label);

std::string gl_load_shader(const char *rel);

GLuint gl_compile_program_files(const char *vs_rel, const char *fs_rel,
                                const char *label);

void gl_check(const char *where);

bool gl_make_current(EGLDisplay display, EGLSurface surface,
                     EGLContext context);
