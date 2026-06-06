#pragma once
// Glowing mushrooms scattered in clusters around the grove. They pulse with light
// and can be "harvested" by flying close — each gives a little warmth and then
// regrows after a while. Stationary cousins of the fireflies; rendered through the
// geometry pass as instanced emissive props.
#include "Render.h"
#include <glm/glm.hpp>
#include <vector>

struct Mushroom {
    glm::vec3 pos = glm::vec3(0);
    glm::vec3 color = glm::vec3(0.4f, 0.9f, 1.0f);
    float phase = 0.0f;
    bool active = true; // glowing & harvestable
    float regrow = 0.0f;
};

class MushroomField {
  public:
    std::vector<Mushroom> mShrooms;

    void init(int clusters);
    void destroy();
    // Harvest mushrooms within harvestRadius of camPos; returns warmth gained.
    float update(float dt, float time, const glm::vec3 &camPos, float harvestRadius);
    void appendLights(std::vector<ilo::OmniLightGPU> &lights, float time, const glm::vec3 &camPos, int maxLights);
    void render(int shaderProgramID, float time);

  private:
    GLuint mVao = 0, mVbo = 0, mIbo = 0, mInstVbo = 0, mTintVbo = 0;
    int mIndexCount = 0;
    unsigned int mRng = 0x00C0FFEEu;
    float randf();
    float randRange(float a, float b);
};
