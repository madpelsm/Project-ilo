#pragma once
// The firefly swarm: instanced emissive "motes", each an animated point light.
// Rendered through the normal geometry pass (it writes albedo + emissive into the
// G-buffer), so the deferred lighting/bloom treats fireflies like any glowing object.
#include "Render.h"
#include "Vertex.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct Firefly {
    float x = 0, z = 0, baseY = 0; // wander position (render y adds a bob)
    glm::vec2 vel = glm::vec2(0);  // horizontal wander velocity
    glm::vec3 color = glm::vec3(1);
    float phase = 0;       // flicker phase offset
    float flickerHz = 5.0; // flicker speed
    bool skittish = false;
    bool active = true;
    float respawnTimer = 0; // counts down while inactive
    float retarget = 0;     // time until the next wander re-target
};

class FireflySystem {
  public:
    std::vector<Firefly> mFlies;
    int mCollectedThisFrame = 0;

    void init(int count);
    void resetAll(const glm::vec3 &camPos); // respawn every mote (game restart)
    void destroy();
    // Advance the simulation; returns the number of fireflies collected this frame
    // (a mote within collectRadius of camPos). collectedFuel sums their fuel reward.
    int update(float dt, float time, const glm::vec3 &camPos, float collectRadius, float &collectedFuel);
    // Push per-mote point lights for the active fireflies nearest the camera.
    void appendLights(std::vector<ilo::OmniLightGPU> &lights, float time, const glm::vec3 &camPos, int maxFireflyLights);
    // Draw the motes (call inside the geometry pass, after opaque meshes).
    void render(int shaderProgramID, float time);

    int aliveCount() const;

  private:
    GLuint mVao = 0, mVbo = 0, mIbo = 0, mInstVbo = 0, mTintVbo = 0;
    int mIndexCount = 0;
    unsigned int mRng = 0x9e3779b9u;

    float randf();
    float randRange(float a, float b);
    glm::vec3 pickColor(bool skittish);
    void spawn(Firefly &f, const glm::vec3 &camPos);
    float bob(const Firefly &f, float time) const;
};
