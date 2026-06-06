#pragma once
#include "Camera.h"
#include "Firefly.h"
#include "GameObject.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

inline void _glCheckError(const char *file, int line) {
    GLenum err;
    while ((err = glGetError()) != 0) {
        printf("GL Error %x @%s:%d\n", err, file, line);
    }
}

#define glCheckError() _glCheckError(__FILE__, __LINE__)

enum class GameState { Intro, Playing, Paused, Won, Lost };

class Window {
    int mWidth, mHeight, baseObjects = 0;
    std::string mTitle;
    bool closed = false;
    bool windowInitialised = false;
    short frames = 0;
    bool vSync = true, fullscreen = false, windowMaximised = false;
    bool mLowSpec = false;
    float lastTime = 0.0f;
    float mMouseSensitivity = 0.0016f;
    float mFOV = 1.4f; // vertical field of view in radians

    // timing
    double mPrevSeconds = 0.0;
    float mDt = 0.0f;
    float mTime = 0.0f; // accumulated game time fed to shaders

  public:
    Camera mCamera;
    SDL_Window *mSDLwindow = nullptr;
    SDL_GLContext glContext;
    SDL_Event event;

    // shader programs
    ShaderProgram geometryProg, lightingProg, brightProg, blurProg, compositeProg, hudProg;

    // render targets + helpers
    ilo::Framebuffer gBuffer, hdrFBO, bloomA, bloomB;
    ilo::ScreenTri tri;
    ilo::LightUBO lightUBO;
    GLuint mBlackTex = 0;
    GLuint mBloomTex = 0; // final blurred bloom texture for the composite
    bool mBloomReady = false;

    // world
    std::vector<Player *> mGameObjects;
    Player *mForest = nullptr;
    Player *mHeart = nullptr;
    FireflySystem mFireflies;
    Hud mHud;

    // lights packed each frame; [0] is the lantern
    std::vector<ilo::OmniLightGPU> mLights;

    // game state
    GameState mState = GameState::Intro;
    float mFuel = 1.0f;    // 0..1 normalised warmth (derived from mFuelW each frame)
    float mFuelW = 70.0f;  // warmth in fuel units
    int mCollected = 0;
    int mTarget = 30;
    float mIntroTimer = 1.5f;
    bool mSprinting = false;
    bool mFreezeFuel = false; // screenshot/testing: hold fuel constant

    // Heart of the Grove
    glm::vec3 mHeartPos = glm::vec3(0.0f, 1.6f, 0.0f);
    glm::vec3 mHeartColor = glm::vec3(1.0f, 0.25f, 0.05f);
    float mHeartEmissive = 0.15f;
    float mHeartP = 0.0f;

    // sky / fog / ambient
    glm::vec3 mAmbient = glm::vec3(0.012f, 0.016f, 0.028f);
    glm::vec3 mFogColor = glm::vec3(0.02f, 0.035f, 0.06f);
    float mFogDensity = 0.035f, mFogHeightFalloff = 0.03f, mFogBaseY = -1.0f;
    glm::vec3 mSkyTop = glm::vec3(0.008f, 0.015f, 0.05f);
    glm::vec3 mSkyHorizon = glm::vec3(0.03f, 0.05f, 0.09f);
    glm::vec3 mMoonDir = glm::vec3(0.35f, 0.55f, -0.45f);
    glm::vec3 mMoonColor = glm::vec3(0.55f, 0.62f, 0.85f);
    float mMoonSize = 0.07f;

    // composite tuning
    float mExposure = 1.0f;
    float mBloomIntensity = 0.55f;
    float mVignetteMax = 0.55f;

    Window();
    Window(int width, int height, std::string title);
    ~Window();
    void sdlDie();
    void init();
    void initGL();
    void initAssets();
    void run();
    void update();
    void render();
    void renderGeometryPass();
    void renderLightingPass();
    void renderBloom();
    void renderComposite();
    void renderHud();
    void checkEvents();
    void resize();
    void createFramebuffers();
    void destroyFramebuffers();
    void loadGeometries();
    void packLights();
    void updateHeart();
    void resetGame();

    void setvSync(bool vSyncStatus);
    void setMouseSensitivity(float _sensitivity);
    void setScrollSensitivity(float _sensitivity);
    void setSSAA(float _SSAAamount);
    void setFOV(float _fov);

    void setCamera(Camera &c);
    void addNPC(Player &npc);
};
