#include <fstream>
#include <sstream>

#include "core/log.h"

#include "render/gl.h"

#ifndef KOKUSEI_SHADER_DIR
#define KOKUSEI_SHADER_DIR ""
#endif

std::string gl_load_shader(const char *rel) {
    const std::string candidates[] = {
        std::string(KOKUSEI_SHADER_DIR) + "/" + rel,
        std::string("assets/shaders/") + rel,
    };
    for (const std::string &path : candidates) {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            continue;
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    klog("shader: cannot read %s", rel);
    return {};
}

GLuint gl_compile_program_files(const char *vs_rel, const char *fs_rel,
                                const char *label) {
    std::string vs = gl_load_shader(vs_rel);
    std::string fs = gl_load_shader(fs_rel);
    if (vs.empty() || fs.empty())
        return 0;
    return gl_compile_program(vs.c_str(), fs.c_str(), label);
}

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
