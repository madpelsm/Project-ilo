#include "Butterflies.h"
#include "Props.h"
#include "Terrain.h"
#include <cmath>

namespace {
// Saturated meadow palette so the butterflies pop against the green.
glm::vec3 palette(float h) {
    if (h < 0.22f)
        return glm::vec3(1.00f, 0.78f, 0.30f); // amber
    if (h < 0.44f)
        return glm::vec3(1.00f, 0.45f, 0.62f); // rose
    if (h < 0.62f)
        return glm::vec3(0.45f, 0.70f, 1.00f); // sky blue
    if (h < 0.80f)
        return glm::vec3(0.78f, 0.50f, 1.00f); // violet
    return glm::vec3(1.00f, 0.95f, 0.85f);     // pale cream
}
} // namespace

void Butterflies::init(int count) {
    // Two wings (fore + hind) per side, hinged on the body line, forward = +Z, with a
    // gentle dihedral so the silhouette reads as a butterfly from above and the side.
    proc::Mesh mesh;
    glm::vec3 col(1.0f, 1.0f, 1.0f), mtl(10.0f, 0.25f, 0.35f);
    glm::vec3 bf(0.0f, 0.0f, 0.12f), bb(0.0f, 0.0f, -0.12f);
    glm::vec3 ltf(-0.34f, 0.05f, 0.18f), lth(-0.30f, 0.04f, -0.22f);
    glm::vec3 rtf(0.34f, 0.05f, 0.18f), rth(0.30f, 0.04f, -0.22f);
    proc::tri(mesh, bf, ltf, lth, col, mtl);
    proc::tri(mesh, bf, lth, bb, col, mtl);
    proc::tri(mesh, bf, rth, rtf, col, mtl);
    proc::tri(mesh, bf, bb, rth, col, mtl);
    mField.buildDynamic(mesh, count);

    unsigned int s = 0x5EED1Eu;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / (float)0x1000000; };
    // Butterflies gather in patches over flower-rich ground rather than spreading evenly,
    // so wandering into a meadow means meeting a whole drifting cloud of them at once.
    mFlit.resize(count);
    int clusters = std::max(1, count / 7);
    int idx = 0;
    for (int c = 0; c < clusters && idx < count; c++) {
        // Find a cluster centre on dry meadow (clear of the central Mere) — retry a few
        // times so patches never end up hovering over open water.
        glm::vec2 centre(0.0f);
        for (int tries = 0; tries < 8; tries++) {
            float ang = rnd() * 6.2831853f;
            float rad = 150.0f + rnd() * 280.0f;
            centre = glm::vec2(std::cos(ang) * rad, std::sin(ang) * rad);
            if (ilo::terrainHeightFast(centre.x, centre.y) > 1.0f)
                break;
        }
        float baseHue = rnd();
        int n = 5 + (int)(rnd() * 5.0f);
        for (int k = 0; k < n && idx < count; k++) {
            Flit &f = mFlit[idx++];
            float jr = rnd() * 7.0f;
            float ja = rnd() * 6.2831853f;
            f.home = centre + glm::vec2(std::cos(ja) * jr, std::sin(ja) * jr);
            f.x = f.home.x + rnd() * 3.0f - 1.5f;
            f.z = f.home.y + rnd() * 3.0f - 1.5f;
            f.baseY = std::max(0.3f, ilo::terrainHeightFast(f.home.x, f.home.y));
            f.phase = rnd() * 6.2831853f;
            f.hue = baseHue + (rnd() - 0.5f) * 0.25f; // a patch shares a rough colour family
            f.hue -= std::floor(f.hue);
            f.speed = 0.7f + rnd() * 0.8f;
            f.yaw = rnd() * 6.2831853f;
            f.bob = 0.0f;
        }
    }
    mFlit.resize(idx);
}

void Butterflies::update(float dt, float time, float nightAmount) {
    float day = 1.0f - nightAmount; // butterflies fold away and rest after dusk
    std::vector<FieldInstance> inst;
    inst.reserve(mFlit.size());
    for (Flit &f : mFlit) {
        float t = time * f.speed + f.phase;
        // Erratic flutter: layered sines give the looping, never-straight path.
        float vx = std::sin(t * 0.9f) + 0.5f * std::sin(t * 2.3f + f.phase);
        float vz = std::cos(t * 1.1f + f.phase) + 0.5f * std::sin(t * 1.7f);
        // Tether to the home flower patch so they don't drift off forever.
        float hx = f.home.x - f.x, hz = f.home.y - f.z;
        float hd = std::sqrt(hx * hx + hz * hz);
        if (hd > 10.0f) {
            vx += hx / hd * 1.8f;
            vz += hz / hd * 1.8f;
        }
        float vlen = std::sqrt(vx * vx + vz * vz) + 1e-4f;
        float spd = 1.4f * f.speed * day; // becalmed at night
        f.x += vx / vlen * spd * dt;
        f.z += vz / vlen * spd * dt;

        // Hover clearly above the tall meadow grass with a fluttering bob, so a drifting
        // cloud reads against the green rather than sinking into the blades.
        f.bob = 1.35f + 0.45f * std::sin(t * 2.5f) + 0.18f * std::sin(t * 6.0f + f.phase);
        float y = f.baseY + f.bob;

        // Smoothly face the direction of travel.
        float target = std::atan2(vx / vlen, vz / vlen);
        float diff = std::fmod(target - f.yaw + 9.42477796f, 6.2831853f) - 3.14159265f;
        f.yaw += diff * std::min(1.0f, 8.0f * dt);

        glm::vec3 c = palette(f.hue);
        FieldInstance fi;
        fi.pos = glm::vec3(f.x, y, f.z);
        // Faint emissive so they shimmer and catch a touch of bloom in sunlight.
        fi.tintEmissive = glm::vec4(c, 0.28f * day);
        // Wing flap folds the span as a fast scale flutter (cheap, but reads alive).
        float flap = 0.78f + 0.22f * std::sin(t * 14.0f);
        fi.xform = glm::vec4(flap * (1.05f * day + 0.001f), f.yaw, 0.0f, 0.0f);
        inst.push_back(fi);
    }
    mField.update(inst);
}

void Butterflies::render(int shaderProgramID) { mField.render(shaderProgramID); }
void Butterflies::destroy() { mField.destroy(); }
