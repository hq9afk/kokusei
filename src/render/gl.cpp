#include "core/log.h"

#include "render/gl.h"

GLuint gl_compile_program(const char *vs_src, const char *fs_src,
                          const char *label) {
    auto compile = [label](GLenum type, const char *src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[512];
            glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
            klog("shader compile failed (%s): %s", label ? label : "?", info);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs)
        return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        klog("program link failed (%s): %s", label ? label : "?", info);
        glDeleteProgram(program);
        return 0;
    }
    klog("gl: linked program %u (%s)", program, label ? label : "?");
    return program;
}

void gl_check(const char *where) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
        klog("gl: %s -> 0x%04x", where ? where : "?", err);
}

bool gl_make_current(EGLDisplay display, EGLSurface surface,
                     EGLContext context) {
    if (!eglMakeCurrent(display, surface, surface, context)) {
        klog("gl: eglMakeCurrent failed, egl error 0x%04x", eglGetError());
        return false;
    }
    if (surface != EGL_NO_SURFACE && !eglSwapInterval(display, 0))
        klog("gl: eglSwapInterval(0) failed, egl error 0x%04x", eglGetError());
    return true;
}
