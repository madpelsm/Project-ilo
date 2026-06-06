#include "Birds.h"
#include "Props.h"
#include <cmath>

void Birds::init(int count) {
    // A simple swept-wing silhouette pointing +Z, ~2m span.
    proc::Mesh mesh;
    glm::vec3 col(0.08f, 0.08f, 0.11f), mtl(6, 0.0f, 0);
    glm::vec3 front(0, 0, 0.55f), back(0, 0, -0.45f);
    glm::vec3 lt(-1.0f, 0.12f, -0.15f), rt(1.0f, 0.12f, -0.15f);
    proc::tri(mesh, front, lt, back, col, mtl);
    proc::tri(mesh, front, back, rt, col, mtl);
    mField.buildDynamic(mesh, count);

    unsigned int s = 0xB17D5u;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / (float)0x1000000; };
    mBirds.resize(count);
    for (int i = 0; i < count; i++) {
        Bird &b = mBirds[i];
        int flock = i < count / 2 ? 0 : 1;
        b.dir = flock == 0 ? 1.0f : -1.0f;
        b.rad = 70.0f + rnd() * 130.0f + flock * 50.0f;
        b.hgt = 34.0f + rnd() * 34.0f;
        b.spd = 7.0f + rnd() * 4.0f;
        b.ang = rnd() * 6.2831853f + flock * 3.14159f;
        b.phase = rnd() * 6.2831853f;
    }
}

void Birds::update(float dt, float time, float nightAmount) {
    std::vector<FieldInstance> inst;
    inst.reserve(mBirds.size());
    for (Bird &b : mBirds) {
        b.ang += b.dir * b.spd * dt / b.rad;
        float wob = std::sin(time * 0.7f + b.phase) * 12.0f; // gentle radius/height drift
        float r = b.rad + wob;
        float x = std::cos(b.ang) * r;
        float z = std::sin(b.ang) * r;
        float y = b.hgt + std::sin(time * 0.5f + b.phase) * 6.0f;
        // face along the orbit tangent
        float vx = -std::sin(b.ang) * b.dir, vz = std::cos(b.ang) * b.dir;
        float yaw = std::atan2(vx, vz);
        FieldInstance fi;
        fi.pos = glm::vec3(x, y, z);
        fi.tintEmissive = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f * nightAmount); // faint glow at night
        fi.xform = glm::vec4(2.8f, yaw, 0.0f, 0.0f);
        inst.push_back(fi);
    }
    mField.update(inst);
}

void Birds::render(int shaderProgramID) { mField.render(shaderProgramID); }
void Birds::destroy() { mField.destroy(); }
