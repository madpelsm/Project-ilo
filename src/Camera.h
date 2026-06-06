#pragma once
#include "GL.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// First-person fly camera driven by explicit yaw/pitch (no accumulated roll).
class Camera {
  public:
    glm::mat4 mView = glm::mat4(1);
    glm::vec3 mUp = glm::vec3(0, 1, 0);
    glm::vec3 mPosition = glm::vec3(0, 2, 0);
    glm::vec3 mLookDir = glm::vec3(0, 0, -1);
    glm::vec3 mTarget = glm::vec3(0, 2, -1);
    float mYaw = 0.0f;   // radians; 0 looks down -Z
    float mPitch = 0.0f; // radians; clamped to +/- mPitchLimit
    float mPitchLimit = 1.50f;
    float mMinY = 0.5f, mMaxY = 120.0f;

    Camera();
    Camera(glm::vec3 position, glm::vec3 direction);
    ~Camera();

    void update(); // recompute mLookDir, mTarget and mView from yaw/pitch/position

    // Rotation (radians already scaled by sensitivity).
    void rotate(float dYaw, float dPitch);

    // Movement (distance already scaled by speed*dt). Horizontal moves stay in XZ.
    void moveForward(float dist);
    void moveRight(float dist);
    void moveUp(float dist);

    void setYawPitch(float yaw, float pitch);

    glm::mat4 view() const { return mView; }
    glm::vec3 position() const { return mPosition; }
};
