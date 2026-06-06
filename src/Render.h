#pragma once
// Small RAII-ish OpenGL helpers used by the deferred renderer.
// They make the framebuffer/texture lifetimes explicit so that resize no longer
// leaks targets, and provide a VBO-less fullscreen triangle and a std140 light UBO.
#include <cstdint>
#include "GL.h"
#include <iostream>
#include <vector>

namespace ilo {

// ---- a single 2D texture render target -------------------------------------
struct Texture2D {
    GLuint id = 0;
    int w = 0, h = 0;

    void create(int width, int height, GLenum internalFmt, GLenum fmt, GLenum type,
                GLenum filter = GL_NEAREST, GLenum wrap = GL_CLAMP_TO_EDGE) {
        destroy();
        w = width;
        h = height;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, width, height, 0, fmt, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void destroy() {
        if (id) {
            glDeleteTextures(1, &id);
            id = 0;
        }
    }
};

// ---- a framebuffer that owns its colour targets and an optional depth RBO ---
// destroy() deletes EVERYTHING it owns, so resize() cannot leak attachments.
struct Framebuffer {
    GLuint fbo = 0;
    std::vector<Texture2D> colors;
    GLuint depthRbo = 0;
    GLuint depthTex = 0; // samplable depth attachment (shadow map)
    int w = 0, h = 0;

    void create(int width, int height) {
        destroy();
        w = width;
        h = height;
        glGenFramebuffers(1, &fbo);
    }
    void bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }

    // Adds a colour attachment at the next free slot. Returns its texture id.
    GLuint addColor(GLenum internalFmt, GLenum fmt, GLenum type,
                    GLenum filter = GL_NEAREST, GLenum wrap = GL_CLAMP_TO_EDGE) {
        Texture2D t;
        t.create(w, h, internalFmt, fmt, type, filter, wrap);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)colors.size(),
                               GL_TEXTURE_2D, t.id, 0);
        colors.push_back(t);
        return t.id;
    }
    void addDepth(GLenum fmt = GL_DEPTH_COMPONENT24) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, fmt, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
    }
    // A SAMPLABLE depth texture set up for hardware shadow comparison (sampler2DShadow,
    // LEQUAL). Makes this a depth-only FBO: no colour, draw/read buffers set to NONE
    // (required for completeness on WebGL2). Returns the texture id.
    GLuint addDepthTexture(GLenum internalFmt = GL_DEPTH_COMPONENT24) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return depthTex;
    }
    GLuint depth() const { return depthTex; }
    void setDrawBuffers() {
        std::vector<GLenum> bufs;
        for (size_t i = 0; i < colors.size(); ++i)
            bufs.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDrawBuffers((GLsizei)bufs.size(), bufs.data());
    }
    bool complete(const char *name = "fbo") const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (s != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Framebuffer '" << name << "' incomplete: 0x" << std::hex << s << std::dec << std::endl;
            return false;
        }
        return true;
    }
    GLuint color(size_t i = 0) const { return colors[i].id; }
    void destroy() {
        for (auto &c : colors)
            c.destroy();
        colors.clear();
        if (depthRbo) {
            glDeleteRenderbuffers(1, &depthRbo);
            depthRbo = 0;
        }
        if (depthTex) {
            glDeleteTextures(1, &depthTex);
            depthTex = 0;
        }
        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }
};

// ---- VBO-less fullscreen triangle (vertices generated from gl_VertexID) -----
struct ScreenTri {
    GLuint vao = 0;
    void init() {
        if (!vao)
            glGenVertexArrays(1, &vao);
    }
    void draw() const {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }
    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    }
};

// ---- std140 point-light block --------------------------------------------
// Mirrors:  struct OmniLight { vec4 posRadius; vec4 colorIntensity; };
//           layout(std140) uniform LightBlock { OmniLight lights[128]; int uLightCount; };
struct OmniLightGPU {
    float posRadius[4];      // xyz = world position, w = radius (metres)
    float colorIntensity[4]; // rgb = linear colour,  w = HDR intensity
};

struct LightUBO {
    static const int MAX_LIGHTS = 128;
    static const int COUNT_OFFSET = MAX_LIGHTS * (int)sizeof(OmniLightGPU); // 4096
    static const int BLOCK_SIZE = COUNT_OFFSET + 16;                        // pad count to 16
    GLuint ubo = 0;

    void init() {
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, BLOCK_SIZE, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    void bindBase(GLuint index) const { glBindBufferBase(GL_UNIFORM_BUFFER, index, ubo); }
    void upload(const OmniLightGPU *lights, int count) {
        if (count > MAX_LIGHTS)
            count = MAX_LIGHTS;
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        if (count > 0)
            glBufferSubData(GL_UNIFORM_BUFFER, 0, count * (int)sizeof(OmniLightGPU), lights);
        glBufferSubData(GL_UNIFORM_BUFFER, COUNT_OFFSET, sizeof(int), &count);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    void destroy() {
        if (ubo) {
            glDeleteBuffers(1, &ubo);
            ubo = 0;
        }
    }
};

} // namespace ilo
