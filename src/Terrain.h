#pragma once
// The land of Lumenmere: a finite caldera bowl — a lake basin at the centre, rolling
// meadows and slopes, and a mountain ridge around the rim. The height is a single
// analytic function (bowl profile + fbm) so the mesh, the camera ground-follow and
// vegetation scatter all agree without a baked texture. One indexed mesh, rendered
// through the normal geometry pass (so it gets deferred lighting + the floating origin).
#include "GL.h"
#include "Vertex.h"
#include <cmath>
#include <vector>

namespace ilo {

// --- shared analytic height field (metres) -------------------------------------
inline float terr_hash(float x, float z) {
    float h = std::sin(x * 127.1f + z * 311.7f) * 43758.5453f;
    return h - std::floor(h);
}
inline float terr_vnoise(float x, float z) {
    float xi = std::floor(x), zi = std::floor(z);
    float xf = x - xi, zf = z - zi;
    float u = xf * xf * (3.0f - 2.0f * xf), v = zf * zf * (3.0f - 2.0f * zf);
    float a = terr_hash(xi, zi), b = terr_hash(xi + 1, zi);
    float c = terr_hash(xi, zi + 1), d = terr_hash(xi + 1, zi + 1);
    return (a + (b - a) * u) + (c - a) * v * (1.0f - u) + (d - b) * u * v;
}
inline float terr_fbm(float x, float z) {
    float s = 0.0f, a = 0.5f;
    for (int i = 0; i < 5; i++) {
        s += a * terr_vnoise(x, z);
        x *= 2.03f;
        z *= 2.03f;
        a *= 0.5f;
    }
    return s;
}
inline float terr_sstep(float a, float b, float x) {
    float t = (x - a) / (b - a);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3.0f - 2.0f * t);
}

// Height of the land at world (x,z). Water sits at y = 0 (the Mere).
inline float terrainHeight(float x, float z) {
    float r = std::sqrt(x * x + z * z);
    float bowl = terr_sstep(60.0f, 700.0f, r) * 16.0f;     // gentle rise across the vale
    float ridge = terr_sstep(680.0f, 880.0f, r) * 95.0f;   // mountain rim
    float basin = -9.0f * terr_sstep(150.0f, 25.0f, r);    // central lake basin (1 at centre)
    float hillAmp = 6.5f * terr_sstep(35.0f, 220.0f, r) * (1.0f - terr_sstep(700.0f, 860.0f, r));
    float hills = (terr_fbm(x * 0.012f, z * 0.012f) - 0.5f) * 2.0f * hillAmp;
    hills += (terr_fbm(x * 0.05f + 13.0f, z * 0.05f) - 0.5f) * hillAmp * 0.4f;
    return bowl + ridge + basin + hills;
}

// Fast height query via a cached, bilerp'd grid (built by Terrain::build). Falls back
// to the analytic terrainHeight before the cache exists. Use this for per-frame queries
// (grass, ground-follow, creatures) — the analytic version is sinf-heavy.
float terrainHeightFast(float x, float z);

class Terrain {
  public:
    void build(float extent, int n); // extent = half-size (m); n = vertices per side
    void render(int shaderProgramID);
    void destroy();
    float worldExtent = 896.0f;

  private:
    GLuint mVao = 0, mVbo = 0, mIbo = 0, mInstVbo = 0, mTintVbo = 0;
    int mIndexCount = 0;
};

} // namespace ilo
