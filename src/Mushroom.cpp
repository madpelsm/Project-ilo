#include "Mushroom.h"
#include "Props.h"
#include "Terrain.h"
#include <algorithm>
#include <cmath>

namespace {
const float WARMTH_PER_MUSHROOM = 12.0f;
const float REGROW_DELAY = 14.0f;
const float GLOW = 2.6f;          // emissive strength of a glowing cap
const float LIGHT_RADIUS = 3.5f;
const float LIGHT_INTENSITY = 1.3f;
const float CULL_DIST = 16.0f;

// Cool bioluminescent palette (contrasts the warm fireflies).
glm::vec3 palette(float r) {
    if (r < 0.40f)
        return glm::vec3(0.35f, 0.90f, 1.00f); // cyan
    if (r < 0.70f)
        return glm::vec3(0.55f, 0.45f, 1.00f); // violet
    if (r < 0.88f)
        return glm::vec3(0.35f, 1.00f, 0.70f); // mint
    return glm::vec3(1.00f, 0.55f, 0.85f);     // pink
}
} // namespace

float MushroomField::randf() {
    mRng ^= mRng << 13;
    mRng ^= mRng >> 17;
    mRng ^= mRng << 5;
    return (mRng & 0xFFFFFF) / (float)0x1000000;
}
float MushroomField::randRange(float a, float b) { return a + (b - a) * randf(); }

void MushroomField::init(int clusters) {
    proc::Mesh mesh = proc::makeMushroom();
    mIndexCount = (int)mesh.size();

    glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);
    glGenBuffers(1, &mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(Vertex2), mesh.data(), GL_STATIC_DRAW);
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
    {
        std::vector<GLuint> idx(mIndexCount);
        for (int i = 0; i < mIndexCount; i++)
            idx[i] = i;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    }
    glBindVertexArray(0);

    // Scatter clusters of mushrooms around the grove (avoiding the very centre).
    for (int c = 0; c < clusters; c++) {
        float ang = randRange(0, 6.2831853f);
        float rad = randRange(130.0f, 520.0f);
        glm::vec3 center(std::cos(ang) * rad, 0, std::sin(ang) * rad);
        glm::vec3 col = palette(randf());
        int n = 3 + (int)(randf() * 4.0f);
        for (int i = 0; i < n; i++) {
            Mushroom s;
            float sx = center.x + randRange(-2.0f, 2.0f);
            float sz = center.z + randRange(-2.0f, 2.0f);
            s.pos = glm::vec3(sx, ilo::terrainHeight(sx, sz), sz); // sit on the ground
            s.color = col;
            s.phase = randRange(0.0f, 6.2831853f);
            mShrooms.push_back(s);
        }
    }
}

void MushroomField::destroy() {
    if (mVao)
        glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
    glDeleteBuffers(1, &mInstVbo);
    glDeleteBuffers(1, &mTintVbo);
    mVao = mVbo = mIbo = mInstVbo = mTintVbo = 0;
}

float MushroomField::update(float dt, float time, const glm::vec3 &camPos, float harvestRadius) {
    float gained = 0.0f;
    float hr2 = harvestRadius * harvestRadius;
    for (auto &s : mShrooms) {
        if (!s.active) {
            s.regrow -= dt;
            if (s.regrow <= 0.0f)
                s.active = true;
            continue;
        }
        float dx = s.pos.x - camPos.x, dy = (s.pos.y + 0.2f) - camPos.y, dz = s.pos.z - camPos.z;
        if (dx * dx + dy * dy + dz * dz < hr2) {
            s.active = false;
            s.regrow = REGROW_DELAY;
            gained += WARMTH_PER_MUSHROOM;
        }
    }
    return gained;
}

void MushroomField::appendLights(std::vector<ilo::OmniLightGPU> &lights, float time, const glm::vec3 &camPos,
                                 int maxLights) {
    struct Cand {
        int i;
        float d2;
    };
    std::vector<Cand> cand;
    for (int i = 0; i < (int)mShrooms.size(); i++) {
        if (!mShrooms[i].active)
            continue;
        float dx = mShrooms[i].pos.x - camPos.x, dz = mShrooms[i].pos.z - camPos.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < CULL_DIST * CULL_DIST)
            cand.push_back({i, d2});
    }
    if ((int)cand.size() > maxLights) {
        std::nth_element(cand.begin(), cand.begin() + maxLights, cand.end(),
                         [](const Cand &a, const Cand &b) { return a.d2 < b.d2; });
        cand.resize(maxLights);
    }
    for (const Cand &c : cand) {
        const Mushroom &s = mShrooms[c.i];
        float pulse = 1.0f + 0.2f * std::sin(6.2831853f * 1.5f * time + s.phase);
        ilo::OmniLightGPU L;
        L.posRadius[0] = s.pos.x;
        L.posRadius[1] = s.pos.y + 0.25f;
        L.posRadius[2] = s.pos.z;
        L.posRadius[3] = LIGHT_RADIUS;
        L.colorIntensity[0] = s.color.x;
        L.colorIntensity[1] = s.color.y;
        L.colorIntensity[2] = s.color.z;
        L.colorIntensity[3] = LIGHT_INTENSITY * pulse;
        lights.push_back(L);
    }
}

float MushroomField::nearestDist(const glm::vec3 &camPos) const {
    float best = 1e18f;
    for (const auto &s : mShrooms) {
        if (!s.active)
            continue;
        float dx = s.pos.x - camPos.x, dy = (s.pos.y + 0.2f) - camPos.y, dz = s.pos.z - camPos.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best)
            best = d2;
    }
    return std::sqrt(best);
}

void MushroomField::render(int shaderProgramID, float time) {
    std::vector<glm::vec3> offsets;
    std::vector<glm::vec4> tints;
    offsets.reserve(mShrooms.size());
    tints.reserve(mShrooms.size());
    for (const Mushroom &s : mShrooms) {
        float glow = 0.0f;
        if (s.active)
            glow = GLOW * (1.0f + 0.2f * std::sin(6.2831853f * 1.5f * time + s.phase));
        offsets.push_back(s.pos);
        tints.push_back(glm::vec4(s.color, glow));
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

    glm::mat4 ident(1.0f);
    glm::mat3 ident3(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, &ident[0][0]);
    glUniformMatrix3fv(glGetUniformLocation(shaderProgramID, "normalMatrix"), 1, GL_FALSE, &ident3[0][0]);
    glUniform1f(glGetUniformLocation(shaderProgramID, "grassWave"), 0.0f);
    glBindVertexArray(mVao);
    glDrawElementsInstanced(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0, (GLsizei)offsets.size());
    glBindVertexArray(0);
}
