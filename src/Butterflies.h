#pragma once
// Butterflies drifting low over the flower meadows — lazy, erratic flutter paths fed
// into an instanced wing mesh rebuilt each frame. They bring the daytime world to life
// the way the wheeling birds own the sky; they fold away and rest as night falls.
#include "InstancedField.h"
#include <glm/glm.hpp>
#include <vector>

class Butterflies {
  public:
    void init(int count);
    void update(float dt, float time, float nightAmount);
    void render(int shaderProgramID);
    void destroy();

  private:
    struct Flit {
        glm::vec2 home;  // the flower patch it haunts
        float x, z;      // current ground position
        float baseY;     // terrain height under the home patch
        float phase;     // desync the wing/bob cycles
        float hue;       // 0..1 into the meadow palette
        float speed;     // wander pace
        float yaw;       // smoothed heading
        float bob;       // current hover height above the flowers
    };
    std::vector<Flit> mFlit;
    InstancedField mField;
};
