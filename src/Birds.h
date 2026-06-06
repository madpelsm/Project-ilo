#pragma once
// Flocks of birds wheeling over the vale — cheap CPU orbits feeding an instanced
// delta-wing mesh that's rebuilt each frame. Silhouettes by day, faint glow by night.
#include "InstancedField.h"
#include <glm/glm.hpp>
#include <vector>

class Birds {
  public:
    void init(int count);
    void update(float dt, float time, float nightAmount);
    void render(int shaderProgramID);
    void destroy();

  private:
    struct Bird {
        float ang, rad, hgt, spd, phase, dir;
    };
    std::vector<Bird> mBirds;
    InstancedField mField;
};
