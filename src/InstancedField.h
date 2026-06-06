#pragma once
// A field of one asset mesh scattered across the world as many instances, each with
// its own position, colour/emissive tint, and a transform (scale, yaw, wind stiffness,
// phase). Drawn in a single instanced call through the geometry pass — the wind sway
// and per-instance variety happen in the vertex shader.
#include "GL.h"
#include "Vertex.h"
#include <glm/glm.hpp>
#include <vector>

struct FieldInstance {
    glm::vec3 pos = glm::vec3(0);
    glm::vec4 tintEmissive = glm::vec4(1, 1, 1, 0); // rgb tint, a emissive strength
    glm::vec4 xform = glm::vec4(1, 0, 0, 0);        // scale, yaw, windStiffness, phase
};

class InstancedField {
  public:
    // mesh = the asset's triangle list (non-indexed); instances = where to scatter it.
    void build(const std::vector<Vertex2> &mesh, const std::vector<FieldInstance> &instances);
    // Dynamic field (e.g. camera-following grass): allocate for maxInstances, refill via update().
    void buildDynamic(const std::vector<Vertex2> &mesh, int maxInstances);
    void update(const std::vector<FieldInstance> &instances);
    void render(int shaderProgramID);
    void destroy();
    int count() const { return mCount; }

  private:
    void setupMesh(const std::vector<Vertex2> &mesh, GLenum instanceUsage, int reserveInstances);
    GLuint mVao = 0, mVbo = 0, mIbo = 0, mOffVbo = 0, mTintVbo = 0, mXformVbo = 0;
    int mIndexCount = 0, mCount = 0, mMax = 0;
};
