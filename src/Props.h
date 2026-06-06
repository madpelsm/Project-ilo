#pragma once
// Procedurally generated, low-poly meshes for the grove — trees, rocks, mushrooms,
// flowers. Everything here is built in code (no asset files) as lists of Vertex2,
// ready to hand to Player::setGeometry / a system's instanced mesh.
#include "Vertex.h"
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

namespace proc {

using Mesh = std::vector<Vertex2>;

// Small deterministic RNG so worlds are reproducible per seed.
struct Rng {
    unsigned int s;
    Rng(unsigned int seed = 1u) : s(seed ? seed : 1u) {}
    float f() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (s & 0xFFFFFF) / (float)0x1000000;
    }
    float range(float a, float b) { return a + (b - a) * f(); }
};

inline void tri(Mesh &m, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
                const glm::vec3 &col, const glm::vec3 &mtl) {
    glm::vec3 n = glm::cross(b - a, c - a);
    float L = glm::length(n);
    n = (L > 1e-6f) ? n / L : glm::vec3(0, 1, 0);
    m.push_back(Vertex2(Vertex(a, col), n, mtl));
    m.push_back(Vertex2(Vertex(b, col), n, mtl));
    m.push_back(Vertex2(Vertex(c, col), n, mtl));
}

// Tapered tube along +Y from base center c0 (radius r0) to top (radius r1, height h).
inline void cylinder(Mesh &m, glm::vec3 c0, float r0, float r1, float h, int seg,
                     glm::vec3 col, glm::vec3 mtl) {
    glm::vec3 c1 = c0 + glm::vec3(0, h, 0);
    for (int i = 0; i < seg; i++) {
        float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
        glm::vec3 b0 = c0 + glm::vec3(std::cos(a0) * r0, 0, std::sin(a0) * r0);
        glm::vec3 b1 = c0 + glm::vec3(std::cos(a1) * r0, 0, std::sin(a1) * r0);
        glm::vec3 t0 = c1 + glm::vec3(std::cos(a0) * r1, 0, std::sin(a0) * r1);
        glm::vec3 t1 = c1 + glm::vec3(std::cos(a1) * r1, 0, std::sin(a1) * r1);
        tri(m, b0, b1, t1, col, mtl);
        tri(m, b0, t1, t0, col, mtl);
    }
}

// Cone with base circle radius r centered at c0, apex h above.
inline void cone(Mesh &m, glm::vec3 c0, float r, float h, int seg, glm::vec3 col, glm::vec3 mtl) {
    glm::vec3 apex = c0 + glm::vec3(0, h, 0);
    for (int i = 0; i < seg; i++) {
        float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
        glm::vec3 b0 = c0 + glm::vec3(std::cos(a0) * r, 0, std::sin(a0) * r);
        glm::vec3 b1 = c0 + glm::vec3(std::cos(a1) * r, 0, std::sin(a1) * r);
        tri(m, b0, b1, apex, col, mtl);
    }
}

// Lumpy low-poly rock around `at`.
inline void rock(Mesh &m, glm::vec3 at, float r, Rng &rng, glm::vec3 col) {
    glm::vec3 mtl(20, 0.3f, 0);
    glm::vec3 top = at + glm::vec3(0, r * rng.range(0.7f, 1.1f), 0);
    glm::vec3 bot = at + glm::vec3(0, -r * 0.3f, 0);
    const int seg = 6;
    glm::vec3 ring[seg];
    for (int i = 0; i < seg; i++) {
        float a = 6.2831853f * i / seg;
        float rr = r * rng.range(0.7f, 1.15f);
        ring[i] = at + glm::vec3(std::cos(a) * rr, r * rng.range(-0.1f, 0.25f), std::sin(a) * rr);
    }
    for (int i = 0; i < seg; i++) {
        glm::vec3 a = ring[i], b = ring[(i + 1) % seg];
        tri(m, a, b, top, col, mtl);
        tri(m, b, a, bot, col, mtl);
    }
}

// A single low-poly conifer (trunk + stacked foliage cones) with its base at `at`.
inline void tree(Mesh &m, glm::vec3 at, Rng &rng) {
    glm::vec3 mtl(8, 0.1f, 0);
    float h = rng.range(2.2f, 4.6f);
    float trunkR = h * 0.045f;
    glm::vec3 trunkCol(rng.range(0.16f, 0.24f), rng.range(0.10f, 0.15f), 0.07f);
    cylinder(m, at, trunkR * 1.25f, trunkR * 0.7f, h * 0.55f, 5, trunkCol, mtl);

    glm::vec3 g(rng.range(0.06f, 0.20f), rng.range(0.30f, 0.55f), rng.range(0.07f, 0.18f));
    int cones = 2 + (rng.f() < 0.6f ? 1 : 0);
    float baseY = h * 0.38f;
    float fr = h * 0.34f;
    for (int i = 0; i < cones; i++) {
        cone(m, at + glm::vec3(0, baseY, 0), fr, h * 0.42f, 6, g, mtl);
        baseY += h * 0.2f;
        fr *= 0.72f;
    }
}

// A grove of trees and rocks scattered in the ring [inner, outer], leaving the
// centre clearing open for the Heart. One static mesh.
inline Mesh makeGroveProps(unsigned int seed, int treeCount, int rockCount, float inner, float outer,
                           float (*heightFn)(float, float) = nullptr) {
    Mesh m;
    Rng rng(seed);
    auto groundY = [&](float x, float z) { return heightFn ? heightFn(x, z) : 0.0f; };
    for (int i = 0; i < treeCount; i++) {
        float ang = rng.range(0, 6.2831853f);
        float rad = rng.range(inner, outer);
        float x = std::cos(ang) * rad, z = std::sin(ang) * rad;
        float y = groundY(x, z);
        if (y < 0.5f)
            continue; // don't plant trees in the lake / on the shoreline
        tree(m, glm::vec3(x, y, z), rng);
    }
    for (int i = 0; i < rockCount; i++) {
        float ang = rng.range(0, 6.2831853f);
        float rad = rng.range(inner * 0.5f, outer);
        float x = std::cos(ang) * rad, z = std::sin(ang) * rad;
        float gray = rng.range(0.08f, 0.16f);
        rock(m, glm::vec3(x, groundY(x, z) - 0.1f, z), rng.range(0.25f, 0.7f), rng,
             glm::vec3(gray, gray, gray * 1.1f));
    }
    return m;
}

// A single mushroom unit (base at origin): dim stem + bright cap. The cap colour
// is multiplied per-instance and the glow comes from the instance's emissive.
inline Mesh makeMushroom() {
    Mesh m;
    glm::vec3 mtl(8, 0.1f, 0);
    cylinder(m, glm::vec3(0, 0, 0), 0.05f, 0.04f, 0.22f, 6, glm::vec3(0.32f, 0.28f, 0.24f), mtl);
    cone(m, glm::vec3(0, 0.17f, 0), 0.17f, 0.17f, 9, glm::vec3(1.0f, 1.0f, 1.0f), mtl);
    return m;
}

} // namespace proc
