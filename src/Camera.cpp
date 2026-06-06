#include "Camera.h"
#include <algorithm>
#include <cmath>

Camera::Camera() {
    update();
}

Camera::Camera(glm::vec3 position, glm::vec3 direction) {
    mPosition = position;
    direction = glm::normalize(direction);
    mPitch = std::asin(std::max(-1.0f, std::min(1.0f, direction.y)));
    mYaw = std::atan2(direction.x, -direction.z);
    update();
}

Camera::~Camera() {
}

void Camera::update() {
    float cp = std::cos(mPitch);
    mLookDir = glm::vec3(cp * std::sin(mYaw), std::sin(mPitch), -cp * std::cos(mYaw));
    mTarget = mPosition + mLookDir;
    mView = glm::lookAt(mPosition, mTarget, mUp);
}

void Camera::rotate(float dYaw, float dPitch) {
    mYaw += dYaw;
    mPitch += dPitch;
    mPitch = std::max(-mPitchLimit, std::min(mPitchLimit, mPitch));
    update();
}

void Camera::setYawPitch(float yaw, float pitch) {
    mYaw = yaw;
    mPitch = std::max(-mPitchLimit, std::min(mPitchLimit, pitch));
    update();
}

void Camera::moveForward(float dist) {
    glm::vec3 fwd = glm::vec3(mLookDir.x, 0.0f, mLookDir.z);
    float len = glm::length(fwd);
    if (len > 1e-5f)
        mPosition += dist * (fwd / len);
    update();
}

void Camera::moveRight(float dist) {
    glm::vec3 fwd = glm::vec3(mLookDir.x, 0.0f, mLookDir.z);
    float len = glm::length(fwd);
    if (len > 1e-5f) {
        fwd /= len;
        mPosition += dist * glm::vec3(-fwd.z, 0.0f, fwd.x);
    }
    update();
}

void Camera::moveUp(float dist) {
    mPosition.y = std::max(mMinY, std::min(mMaxY, mPosition.y + dist));
    update();
}
