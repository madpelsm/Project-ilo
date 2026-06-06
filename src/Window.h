#pragma once
#include "Camera.h"
#include "Firefly.h"
#include "GameObject.h"
#include "Hud.h"
#include "Mushroom.h"
#include "Player.h"
#include "Render.h"
#include "Sky.h"
#include "Terrain.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include <SDL2/SDL.h>
#include "GL.h"
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
    bool mDestroyed = false;
    bool windowInitialised = false;
    short frames = 0;
    bool vSync = true, fullscreen = false, windowMaximised = false;
    bool mLowSpec = false;
    float lastTime = 0.0f;
    float mMouseSensitivity = 0.0016f;
    bool mInvertY = false; // toggle with 'I' (handy for trackpads)
    float mFOV = 1.4f; // vertical field of view in radians

    // timing
    double mPrevSeconds = 0.0;
    float mDt = 0.0f;
    float mTime = 0.0f; // accumulated game time fed to shaders

    // headless capture (read the back buffer before presenting)
    bool mPendingShot = false;
    std::string mShotPath;

  public:
    Camera mCamera;
    SDL_Window *mSDLwindow = nullptr;
    SDL_GLContext glContext;
    SDL_Event event;

    // shader programs (HUD owns its own program)
    ShaderProgram geometryProg, lightingProg, brightProg, blurProg, compositeProg, skyProg;

    // render targets + helpers
    ilo::Framebuffer gBuffer, hdrFBO, bloomA, bloomB, skyFBO;
    ilo::ScreenTri tri;
    ilo::LightUBO lightUBO;
    GLuint mBlackTex = 0;
    GLuint mBloomTex = 0; // final blurred bloom texture for the composite
    bool mBloomReady = false;

    // world
    std::vector<Player *> mGameObjects;
    Player *mForest = nullptr;
    Player *mHeart = nullptr;
    Player *mProps = nullptr; // procedural trees + rocks (double-sided)
    ilo::Terrain mTerrain;
    FireflySystem mFireflies;
    MushroomField mMushrooms;
    Hud mHud;
    float mEyeOffset = 1.8f; // camera height above the ground (rises when flying)
    bool mFreeCam = false;   // disable ground-follow (aerial screenshots)

    // lights packed each frame; [0] is the lantern
    std::vector<ilo::OmniLightGPU> mLights;

    // game state
    GameState mState = GameState::Intro;
    Sky mSky;
    glm::vec3 mRenderOrigin = glm::vec3(0); // floating-origin (snapped to a 128m grid)
    float mDayPhase = 0.0f;     // 0 midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset
    float mDayLength = 1080.0f; // seconds for a full day-night cycle (~18 min)
    float mFuel = 1.0f;    // 0..1 normalised warmth (derived from mFuelW each frame)
    float mFuelW = 70.0f;  // warmth in fuel units
    int mCollected = 0;
    int mTarget = 30;
    float mIntroTimer = 1.5f;
    bool mSprinting = false;
    bool mFreezeFuel = false; // screenshot/testing: hold fuel constant
    bool mFreezeDay = false;  // screenshot/testing: hold the day phase constant
    int mCombo = 0;           // consecutive firefly catches
    float mComboTimer = 0.0f; // time left to extend the combo
    float mDawn = 0.0f;       // 0 = night, 1 = full dawn (rises with progress / on win)

    // Heart of the Grove
    glm::vec3 mHeartPos = glm::vec3(0.0f, 1.6f, 0.0f);
    glm::vec3 mHeartColor = glm::vec3(1.0f, 0.25f, 0.05f);
    float mHeartEmissive = 0.15f;
    float mHeartP = 0.0f;

    // Juice
    struct Pulse {
        glm::vec3 pos;
        glm::vec3 color;
        float age;
        float life;
    };
    std::vector<Pulse> mPulses;
    float mFlash = 0.0f;         // brief white screen flash on collect
    float mFlareTimer = 0.0f;    // lantern flare remaining
    float mFlareCooldown = 0.0f; // time until the next flare is allowed
    bool mMoving = false;

    struct DeerAgent {
        Player *p = nullptr;
        float x = 0, z = 0, tx = 0, tz = 0, yaw = 0, pause = 0;
    };
    std::vector<DeerAgent> mDeer;

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
    float mBloomIntensity = 0.68f;
    float mVignetteMax = 0.55f;

    Window();
    Window(int width, int height, std::string title);
    ~Window();
    void sdlDie();
    void init();
    void initGL();
    void initAssets();
    void run();
    void stepFrameWeb(); // one frame, driven by the browser main loop
    void update();
    glm::mat4 projection() const {
        return glm::perspective(mFOV, mWidth / (float)(mHeight > 0 ? mHeight : 1), 0.4f, 700.0f);
    }
    void render();
    void renderSky();
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
    void updateDeer();
    void resetGame();
    void addDeer(Player &deer, float x, float z);

    void setvSync(bool vSyncStatus);
    void setMouseSensitivity(float _sensitivity);
    void setScrollSensitivity(float _sensitivity);
    void setSSAA(float _SSAAamount);
    void setFOV(float _fov);

    void setCamera(Camera &c);
    void addNPC(Player &npc);
};
