#pragma once
// Headless screenshot helper for Project Ilo.
// Reads the default framebuffer back into CPU memory and writes a binary PPM
// (P6) image, flipped to top-down orientation. PPM keeps this dependency-free;
// a PNG can be produced offline. Used by the automated verification harness.
#include <cstdint>
#include <cstdio>
#include <glad/glad.h>
#include <string>
#include <vector>

namespace ilo {

inline bool savePPM(const std::string &path, int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    std::vector<unsigned char> pixels((size_t)width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    // OpenGL origin is bottom-left; flip rows so the image is top-down.
    for (int y = height - 1; y >= 0; --y) {
        std::fwrite(pixels.data() + (size_t)y * width * 3, 1, (size_t)width * 3, f);
    }
    std::fclose(f);
    return true;
}

} // namespace ilo
