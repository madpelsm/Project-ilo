#include "Terrain.h"
#include <glm/glm.hpp>
#include <vector>

namespace ilo {

// --- cached heightfield for cheap per-frame queries ---
namespace {
std::vector<float> gCache;
int gCacheN = 0;
float gCacheExtent = 0.0f, gCacheStep = 2.0f;
} // namespace

float terrainHeightFast(float x, float z) {
    if (gCacheN <= 1)
        return terrainHeight(x, z);
    float fx = (x + gCacheExtent) / gCacheStep, fz = (z + gCacheExtent) / gCacheStep;
    if (fx < 0.0f || fz < 0.0f || fx >= gCacheN - 1 || fz >= gCacheN - 1)
        return terrainHeight(x, z);
    int ix = (int)fx, iz = (int)fz;
    float tx = fx - ix, tz = fz - iz;
    const float *c = gCache.data();
    float h00 = c[iz * gCacheN + ix], h10 = c[iz * gCacheN + ix + 1];
    float h01 = c[(iz + 1) * gCacheN + ix], h11 = c[(iz + 1) * gCacheN + ix + 1];
    return (h00 * (1 - tx) + h10 * tx) * (1 - tz) + (h01 * (1 - tx) + h11 * tx) * tz;
}

static void buildHeightCache(float extent) {
    gCacheExtent = extent;
    gCacheStep = 2.0f;
    gCacheN = (int)(2.0f * extent / gCacheStep) + 2;
    gCache.assign((size_t)gCacheN * gCacheN, 0.0f);
    for (int j = 0; j < gCacheN; j++)
        for (int i = 0; i < gCacheN; i++)
            gCache[(size_t)j * gCacheN + i] = terrainHeight(-extent + i * gCacheStep, -extent + j * gCacheStep);
}

// Colour the land by height and slope: shore sand -> meadow grass -> rock -> snow.
static glm::vec3 terrainColor(float x, float z, float h, const glm::vec3 &n) {
    float slope = 1.0f - n.y; // 0 flat .. 1 vertical
    glm::vec3 sand(0.20f, 0.18f, 0.14f);
    glm::vec3 grassLo(0.06f, 0.20f, 0.10f), grassHi(0.10f, 0.30f, 0.14f);
    glm::vec3 rock(0.10f, 0.10f, 0.12f);
    glm::vec3 snow(0.55f, 0.60f, 0.72f);

    float tint = terr_fbm(x * 0.03f, z * 0.03f);
    glm::vec3 grass = grassLo + (grassHi - grassLo) * tint;

    glm::vec3 c = grass;
    c = glm::mix(sand, c, terr_sstep(-0.5f, 2.0f, h)); // shoreline sand near water
    c = glm::mix(c, rock, terr_sstep(0.45f, 0.75f, slope)); // steep -> rock
    c = glm::mix(c, snow, terr_sstep(62.0f, 95.0f, h));     // peaks -> snow
    return c;
}

void Terrain::build(float extent, int n) {
    worldExtent = extent;
    buildHeightCache(extent); // cheap bilerp queries for grass/ground-follow/creatures
    std::vector<Vertex2> verts;
    verts.reserve((size_t)n * n);
    const glm::vec3 mtl(10.0f, 0.06f, 0.0f); // low shininess, faint spec
    float step = (2.0f * extent) / (n - 1);
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            float x = -extent + i * step;
            float z = -extent + j * step;
            float h = terrainHeight(x, z);
            // smooth normal from central differences of the height field
            float e = step;
            float hl = terrainHeight(x - e, z), hr = terrainHeight(x + e, z);
            float hd = terrainHeight(x, z - e), hu = terrainHeight(x, z + e);
            glm::vec3 nrm = glm::normalize(glm::vec3(hl - hr, 2.0f * e, hd - hu));
            glm::vec3 col = terrainColor(x, z, h, nrm);
            verts.push_back(Vertex2(Vertex(glm::vec3(x, h, z), col), nrm, mtl));
        }
    }

    std::vector<GLuint> idx;
    idx.reserve((size_t)(n - 1) * (n - 1) * 6);
    for (int j = 0; j < n - 1; j++) {
        for (int i = 0; i < n - 1; i++) {
            GLuint a = j * n + i, b = j * n + i + 1, c = (j + 1) * n + i, d = (j + 1) * n + i + 1;
            idx.push_back(a); idx.push_back(c); idx.push_back(b);
            idx.push_back(b); idx.push_back(c); idx.push_back(d);
        }
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

    // One static instance (offset 0, no tint/emissive) for the shared geometry shader.
    glm::vec3 zero(0.0f);
    glm::vec4 white(1, 1, 1, 0);
    glGenBuffers(1, &mInstVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mInstVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(zero), &zero, GL_STATIC_DRAW);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glVertexAttribDivisor(4, 1);
    glGenBuffers(1, &mTintVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mTintVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(white), &white, GL_STATIC_DRAW);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(5, 1);

    glGenBuffers(1, &mIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void Terrain::render(int shaderProgramID) {
    if (!mVao)
        return;
    glm::mat4 ident(1.0f);
    glm::mat3 ident3(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, &ident[0][0]);
    glUniformMatrix3fv(glGetUniformLocation(shaderProgramID, "normalMatrix"), 1, GL_FALSE, &ident3[0][0]);
    glUniform1f(glGetUniformLocation(shaderProgramID, "grassWave"), 0.0f);
    glBindVertexArray(mVao);
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Terrain::destroy() {
    if (mVao)
        glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
    glDeleteBuffers(1, &mInstVbo);
    glDeleteBuffers(1, &mTintVbo);
    mVao = mVbo = mIbo = mInstVbo = mTintVbo = 0;
}

} // namespace ilo
