#include "Firefly.h"
#include <algorithm>
#include <cmath>

namespace {
// Tuning (from the design spec).
const float AREA = 18.0f;        // motes roam X,Z in [-AREA, AREA]
const float WANDER_SPEED = 0.6f;
const float BOB_AMPLITUDE = 0.25f;
const float BOB_FREQ = 0.8f;
const float MOTE_SCALE = 0.08f;
const float SPAWN_Y_MIN = 1.0f, SPAWN_Y_MAX = 3.0f;
const float RESPAWN_DELAY = 6.0f;
const float SKITTISH_FRACTION = 0.30f;
const float FLEE_TRIGGER = 4.0f;
const float FLEE_SPEED = 2.2f;
const float FF_LIGHT_RADIUS = 4.0f;
const float FF_LIGHT_INTENSITY = 1.5f;
const float BASE_EMISSIVE = 2.2f; // pushed above the bloom threshold so motes glow
const float LIGHT_CULL_DIST = 16.0f;
} // namespace

float FireflySystem::randf() {
    // xorshift32 -> [0,1)
    mRng ^= mRng << 13;
    mRng ^= mRng >> 17;
    mRng ^= mRng << 5;
    return (mRng & 0xFFFFFF) / (float)0x1000000;
}
float FireflySystem::randRange(float a, float b) { return a + (b - a) * randf(); }

glm::vec3 FireflySystem::pickColor(bool skittish) {
    if (skittish)
        return glm::vec3(0.80f, 0.90f, 1.00f); // cool, near-white
    float r = randf();
    if (r < 0.50f)
        return glm::vec3(1.00f, 0.85f, 0.40f); // golden
    if (r < 0.80f)
        return glm::vec3(0.50f, 1.00f, 0.60f); // green
    if (r < 0.92f)
        return glm::vec3(0.40f, 0.80f, 1.00f); // cyan
    return glm::vec3(1.00f, 0.60f, 0.80f);     // pink
}

void FireflySystem::spawn(Firefly &f, const glm::vec3 &camPos) {
    // Place away from the camera and away from the Heart at the origin.
    for (int tries = 0; tries < 16; ++tries) {
        f.x = randRange(-AREA, AREA);
        f.z = randRange(-AREA, AREA);
        float dHeart = std::sqrt(f.x * f.x + f.z * f.z);
        float dx = f.x - camPos.x, dz = f.z - camPos.z;
        float dCam = std::sqrt(dx * dx + dz * dz);
        if (dHeart > 3.0f && dCam > 4.0f)
            break;
    }
    f.baseY = randRange(SPAWN_Y_MIN, SPAWN_Y_MAX);
    f.skittish = randf() < SKITTISH_FRACTION;
    f.color = pickColor(f.skittish);
    f.phase = randRange(0.0f, 6.2831f);
    f.flickerHz = f.skittish ? 7.0f : 5.0f;
    float ang = randRange(0.0f, 6.2831f);
    f.vel = glm::vec2(std::cos(ang), std::sin(ang)) * WANDER_SPEED;
    f.retarget = randRange(1.5f, 3.0f);
    f.active = true;
    f.respawnTimer = 0;
}

float FireflySystem::bob(const Firefly &f, float time) const {
    return BOB_AMPLITUDE * std::sin(6.2831f * BOB_FREQ * time + f.phase);
}

void FireflySystem::init(int count) {
    // Build a small octahedron mote mesh (flat-shaded), reused for every instance.
    glm::vec3 v[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    int faces[8][3] = {{2, 4, 0}, {2, 0, 5}, {2, 5, 1}, {2, 1, 4},
                       {3, 0, 4}, {3, 5, 0}, {3, 1, 5}, {3, 4, 1}};
    std::vector<Vertex2> verts;
    std::vector<GLuint> idx;
    for (auto &fc : faces) {
        glm::vec3 a = v[fc[0]] * MOTE_SCALE, b = v[fc[1]] * MOTE_SCALE, c = v[fc[2]] * MOTE_SCALE;
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        glm::vec3 white(1.0f);
        glm::vec3 mtl(8.0f, 0.0f, 0.0f); // low shininess, no spec
        GLuint base = (GLuint)verts.size();
        verts.push_back(Vertex2(Vertex(a, white), n, mtl));
        verts.push_back(Vertex2(Vertex(b, white), n, mtl));
        verts.push_back(Vertex2(Vertex(c, white), n, mtl));
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }
    mIndexCount = (int)idx.size();

    glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);
    glGenBuffers(1, &mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex2), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)sizeof(glm::vec3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(2 * sizeof(glm::vec3)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(3 * sizeof(glm::vec3)));

    glGenBuffers(1, &mInstVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mInstVbo);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glVertexAttribDivisor(4, 1);

    glGenBuffers(1, &mTintVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mTintVbo);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(5, 1);

    glGenBuffers(1, &mIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    mFlies.resize(count);
    glm::vec3 spawnCam(0, 2, 18);
    for (auto &f : mFlies)
        spawn(f, spawnCam);
}

void FireflySystem::resetAll(const glm::vec3 &camPos) {
    for (auto &f : mFlies)
        spawn(f, camPos);
}

void FireflySystem::destroy() {
    if (mVao)
        glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
    glDeleteBuffers(1, &mInstVbo);
    glDeleteBuffers(1, &mTintVbo);
    mVao = mVbo = mIbo = mInstVbo = mTintVbo = 0;
}

int FireflySystem::update(float dt, float time, const glm::vec3 &camPos, float collectRadius, float &collectedFuel,
                          std::vector<CollectEvent> *events) {
    int collected = 0;
    collectedFuel = 0.0f;
    float cr2 = collectRadius * collectRadius;

    for (auto &f : mFlies) {
        if (!f.active) {
            f.respawnTimer -= dt;
            if (f.respawnTimer <= 0.0f)
                spawn(f, camPos);
            continue;
        }

        float dxc = f.x - camPos.x;
        float dzc = f.z - camPos.z;
        float dyc = (f.baseY + bob(f, time)) - camPos.y;
        float d2 = dxc * dxc + dyc * dyc + dzc * dzc;

        // Collection.
        if (d2 < cr2) {
            f.active = false;
            f.respawnTimer = RESPAWN_DELAY;
            collected++;
            collectedFuel += f.skittish ? 27.0f : 18.0f;
            if (events)
                events->push_back({glm::vec3(f.x, f.baseY + bob(f, time), f.z), f.color, f.skittish});
            continue;
        }

        // Skittish flee.
        bool fleeing = false;
        if (f.skittish) {
            float dh = std::sqrt(dxc * dxc + dzc * dzc);
            if (dh < FLEE_TRIGGER && dh > 1e-3f) {
                glm::vec2 away = glm::vec2(dxc, dzc) / dh;
                f.vel = away * FLEE_SPEED;
                fleeing = true;
            }
        }

        if (!fleeing) {
            f.retarget -= dt;
            if (f.retarget <= 0.0f) {
                float ang = randRange(0.0f, 6.2831f);
                glm::vec2 target = glm::vec2(std::cos(ang), std::sin(ang)) * WANDER_SPEED;
                f.vel = glm::mix(f.vel, target, 0.5f);
                f.retarget = randRange(2.0f, 3.5f);
            }
        }

        f.x += f.vel.x * dt;
        f.z += f.vel.y * dt;
        // Bounce off the play-area boundary.
        if (f.x < -AREA) { f.x = -AREA; f.vel.x = std::abs(f.vel.x); }
        if (f.x > AREA)  { f.x = AREA;  f.vel.x = -std::abs(f.vel.x); }
        if (f.z < -AREA) { f.z = -AREA; f.vel.y = std::abs(f.vel.y); }
        if (f.z > AREA)  { f.z = AREA;  f.vel.y = -std::abs(f.vel.y); }
    }
    mCollectedThisFrame = collected;
    return collected;
}

void FireflySystem::appendLights(std::vector<ilo::OmniLightGPU> &lights, float time, const glm::vec3 &camPos,
                                 int maxFireflyLights) {
    // Gather active motes near the camera, then light the nearest ones up to the cap.
    struct Cand { int i; float d2; };
    std::vector<Cand> cand;
    cand.reserve(mFlies.size());
    for (int i = 0; i < (int)mFlies.size(); ++i) {
        const Firefly &f = mFlies[i];
        if (!f.active)
            continue;
        float dx = f.x - camPos.x, dz = f.z - camPos.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < LIGHT_CULL_DIST * LIGHT_CULL_DIST)
            cand.push_back({i, d2});
    }
    if ((int)cand.size() > maxFireflyLights) {
        std::nth_element(cand.begin(), cand.begin() + maxFireflyLights, cand.end(),
                         [](const Cand &a, const Cand &b) { return a.d2 < b.d2; });
        cand.resize(maxFireflyLights);
    }
    for (const Cand &c : cand) {
        const Firefly &f = mFlies[c.i];
        float flick = 1.0f + 0.25f * std::sin(6.2831f * f.flickerHz * time + f.phase);
        ilo::OmniLightGPU L;
        L.posRadius[0] = f.x;
        L.posRadius[1] = f.baseY + bob(f, time);
        L.posRadius[2] = f.z;
        L.posRadius[3] = FF_LIGHT_RADIUS;
        L.colorIntensity[0] = f.color.x;
        L.colorIntensity[1] = f.color.y;
        L.colorIntensity[2] = f.color.z;
        L.colorIntensity[3] = FF_LIGHT_INTENSITY * flick;
        lights.push_back(L);
    }
}

void FireflySystem::render(int shaderProgramID, float time) {
    // Build instance data for the active motes.
    std::vector<glm::vec3> offsets;
    std::vector<glm::vec4> tints;
    offsets.reserve(mFlies.size());
    tints.reserve(mFlies.size());
    for (const Firefly &f : mFlies) {
        if (!f.active)
            continue;
        float flick = 1.0f + 0.25f * std::sin(6.2831f * f.flickerHz * time + f.phase);
        offsets.push_back(glm::vec3(f.x, f.baseY + bob(f, time), f.z));
        tints.push_back(glm::vec4(f.color, BASE_EMISSIVE * flick));
    }
    if (offsets.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, mInstVbo);
    glBufferData(GL_ARRAY_BUFFER, offsets.size() * sizeof(glm::vec3), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, offsets.size() * sizeof(glm::vec3), offsets.data());
    glBindBuffer(GL_ARRAY_BUFFER, mTintVbo);
    glBufferData(GL_ARRAY_BUFFER, tints.size() * sizeof(glm::vec4), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, tints.size() * sizeof(glm::vec4), tints.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Motes are tiny double-sided diamonds; skip face culling to dodge winding issues.
    glm::mat4 ident = glm::mat4(1.0f);
    glm::mat3 ident3 = glm::mat3(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, &ident[0][0]);
    glUniformMatrix3fv(glGetUniformLocation(shaderProgramID, "normalMatrix"), 1, GL_FALSE, &ident3[0][0]);
    glUniform1f(glGetUniformLocation(shaderProgramID, "grassWave"), 0.0f);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(mVao);
    glDrawElementsInstanced(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0, (GLsizei)offsets.size());
    glBindVertexArray(0);
}

int FireflySystem::aliveCount() const {
    int n = 0;
    for (const auto &f : mFlies)
        if (f.active)
            n++;
    return n;
}
