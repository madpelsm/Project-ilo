#pragma once
#include "Birds.h"
#include "Butterflies.h"
#include "Camera.h"
#include "Firefly.h"
#include "GameObject.h"
#include "Hud.h"
#include "InstancedField.h"
#include "Mushroom.h"
#include "Player.h"
#include "Render.h"
#include "Save.h"
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
    SDL_GLContext glContext = nullptr;
    SDL_Event event;

    // shader programs (HUD owns its own program)
    ShaderProgram geometryProg, lightingProg, brightProg, blurProg, compositeProg, skyProg, waterProg, godrayProg, particleProg;
    ShaderProgram ssaoProg, ssaoBlurProg;
    ShaderProgram shadowProg; // sun shadow-map depth pass
    ShaderProgram reflectionProg; // planar water reflection pass
    glm::mat4 mReflVP = glm::mat4(1.0f);
    bool mNoReflect = false;
    ShaderProgram ssrProg; // screen-space reflections for the wet shore
    int mSSRSteps = 16;
    float mSSRStride = 1.5f, mSSRThickness = 1.6f;
    bool mNoSSR = false;
    int mSSRDebug = 0;
    std::vector<glm::vec3> mSsaoKernel; // hemisphere offsets, uploaded once
    std::vector<glm::vec3> mSsaoNoise;  // 4x4 rotation tile, uploaded once
    // Sun shadow map (directional, camera-centred, texel-snapped, origin-relative).
    int mShadowRes = 2048;
    float mShadowRadius = 128.0f; // ortho half-extent (metres)
    glm::mat4 mLightVP = glm::mat4(1.0f);
    float mShadowStrength = 0.0f; // sun-elevation/night fade (0 = off)
    int mShadowDebug = 0;
    bool mNoShadow = false;
    // Far cascade: a second, 4x-wider sun map so distant ridge-trees cast too.
    int mShadowResFar = 1024;
    float mShadowRadiusFar = 512.0f;
    glm::mat4 mLightVPFar = glm::mat4(1.0f);
    bool mNoFarShadow = false;
    // Volumetric ground-mist (pools in the hollows; CPU-baked tiling fbm noise).
    ilo::Texture2D mNoiseTex;
    float mMistBaseY = 0.5f, mMistHeightFalloff = 0.4f, mMistStrength = 1.0f;
    // Contact-hardening (PCSS-lite) soft sun shadows.
    GLuint mShadowRawSampler = 0; // no-compare sampler for the blocker search
    float mShadowSunSize = 0.06f, mShadowMaxPenumbra = 0.02f;
    int mShadowTaps = 12;
    bool mNoPcss = false;
    GLuint mWaterVao = 0, mWaterVbo = 0;
    GLuint mParticleVao = 0, mParticleVbo = 0;
    int mParticleCount = 0;

    // render targets + helpers
    ilo::Framebuffer gBuffer, hdrFBO, bloomA, bloomB, skyFBO, godrayFBO, ssaoFBO, ssaoBlurFBO, shadowFBO, shadowFarFBO, reflectionFBO, ssrFBO;
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
    std::vector<InstancedField> mFields; // scattered living vegetation (trees/flowers/rocks)
    InstancedField mGrass;               // dense grass that follows the camera
    // Heartwood beacons: sleeping crystal spires you wake to bloom a region.
    struct Beacon {
        glm::vec3 pos = glm::vec3(0);
        bool lit = false;
        float igniteTime = 0.0f;
    };
    std::vector<Beacon> mBeacons;
    InstancedField mBeaconField;
    std::vector<glm::vec4> mWakeEvents; // (centreX, centreZ, igniteTime, 0)
    int mGrovesAwake = 0;
    std::vector<glm::vec4> mDimples; // fish rises on the Mere: (x, z, spawnTime, 0)
    float mDimpleTimer = 0.0f;       // countdown to the next rise
    unsigned int mDimpleRng = 0x9E3779B9u;

    // Phase 8 — "the sky you author": weave seed-stars into constellations that draw a
    // line across the live sky, ignite a fallen-twin on the plain, colour the aurora,
    // grant a gentle boon, and persist as a record of play.
    static const int MAX_PINS = 6;
    static const int MAX_CONSTELLATIONS = 4;
    static const int MAX_CSTARS = 24;
    struct Constellation {
        std::vector<glm::vec3> stars; // unit sky directions
        glm::vec3 color = glm::vec3(1.0f);
        float bornTime = 0.0f;
        std::vector<glm::vec3> ground; // fallen-twin marker positions (absolute world)
    };
    std::vector<glm::vec3> mWeavePins;            // in-progress pins (unit dirs)
    glm::vec3 mWeaveColor = glm::vec3(0.21f, 1.0f, 0.76f);
    std::vector<Constellation> mConstellations;   // persisted, woven figures
    InstancedField mTwinField;                    // fallen-twin ground markers (beacon mesh)
    glm::vec3 mAuroraColor = glm::vec3(0.10f, 1.0f, 0.5f);
    float mAuroraColorMix = 0.0f;                 // eased toward a target on weave
    float mAuroraColorTarget = 0.0f;
    float mWeaveAuroraBoost = 0.0f;               // decaying surge on completion
    float mBoonLantern = 1.0f;                    // lantern reach/brightness (cap 1.5)
    float mGlideCap = 90.0f;                      // glide ceiling (cap 130)
    glm::vec2 mGrassCenter = glm::vec2(1e9f, 1e9f);
    FireflySystem mFireflies;
    MushroomField mMushrooms;
    Birds mBirds;
    Butterflies mButterflies;
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
    float mRadiance = 0.0f; // 0..1 meta-progress; opens the world dim -> radiant
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
        int herd = 0;        // deer sharing a herd drift and graze together
        float yawOffset = 0; // model's forward axis correction (set per species)
        float trust = 0.0f;  // 0 wary .. 1 follows at your heel (Phase 9)
        float fleeTimer = 0.0f; // >0 while trotting away after being rushed
    };
    std::vector<DeerAgent> mDeer;

    // Phase 9 — falling-leaf glide + the befriended herd + wind-rivers.
    float mVertVel = 0.0f;             // vertical velocity of the height-above-ground
    bool mAirborne = false;
    glm::vec2 mGlideVel = glm::vec2(0); // carried horizontal drift while aloft
    bool mAscendInput = false, mDescendInput = false;
    glm::vec2 mPrevCamXZ = glm::vec2(0);
    bool mPrevCamInit = false;
    float mPlayerSpeed = 0.0f;          // horizontal m/s (deer read this as calm/rushing)
    struct WindRiver {
        glm::vec3 a = glm::vec3(0), b = glm::vec3(0); // lane endpoints (world)
    };
    std::vector<WindRiver> mWindRivers;
    InstancedField mWindMotes;          // drifting motes that make a lane visible

    // Phase 10 — Rest, Memory & The Long Dawn (the soft, endless ending).
    enum class Dawn { None, Flaring, Settled };
    Dawn mLongDawn = Dawn::None;        // orthogonal to GameState — climax never leaves Playing
    float mDawnT = 0.0f;               // seconds since the Long Dawn broke
    float mBloom = 0.0f;               // global unison-flare emissive (-> uBloom)
    float mDawnChorus = 0.0f;          // ramps at the flare, decays (drives the rising motes)
    bool mResting = false;             // Rest/Observe: seated, HUD hidden, time scrubbable
    float mRestDrop = 0.0f;            // eased seated eye-drop
    int mScrubDir = 0;                 // -1/+1 while scrubbing the day in Rest
    bool mJournalShot = false;         // capture a clean (HUD-less) keepsake this frame
    std::string mJournalPath;
    int mJournalSeq = 0;
    float mPhotoNote = 0.0f;           // fading "a moment kept" confirmation
    bool mSeedPresent = false;         // a glowing seed waits at the Mere after The Long Dawn
    glm::vec3 mSeedPos = glm::vec3(0);
    int mSeedsPlanted = 0;
    bool mPlantRequested = false;
    InstancedField mSeedField;         // the single breathing seed at the water
    InstancedField mSeedPatchField;    // the fresh patch a planted seed blooms
    bool mPersistEnabled = false;      // "the vale remembers" (gated by ILO_SAVE in headless)
    bool mSaveDirty = false;
    float mSaveTimer = 0.0f;
    std::string mSavePath;
    int mDemoDeer = 0;                  // headless: 1 = a following companion, 2 = rush test
    bool mDemoGlide = false;            // headless: drive the glide to prove leaf-fall
    bool mDemoGlideSeeded = false;
    int mDbgFrame = 0;
    // Slow-drifting anchor each herd grazes around, so the deer read as a group
    // moving through the meadow rather than wandering at random.
    std::vector<glm::vec2> mHerdAnchors;

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
    void renderShadowPass();
    void renderReflection();
    void initMistNoise();
    void renderGeometryPass();
    void renderSSAO();
    void renderSSR();
    void renderLightingPass();
    void renderWater();
    void renderGodrays();
    void renderParticles();
    void renderBloom();
    void renderComposite();
    void renderHud();
    void checkEvents();
    void resize();
    void createFramebuffers();
    void destroyFramebuffers();
    void loadGeometries();
    void scatterWorld();
    void updateGrass(bool force);
    void packLights();
    void updateHeart();
    void updateDeer();
    void updateGlide();        // falling-leaf vertical + airborne drift + wind-rivers
    void updateWindMotes();    // refresh the mote ribbons that mark the lanes
    void updateLongDawn();     // the climax: world-wide unison bloom + permanent settle
    void enterRest();
    void exitRest();
    void captureJournal();     // a clean HUD-less keepsake photo
    void plantSeed();
    void reseedSeedPatch();
    void loadSession();        // "the vale remembers"
    void saveSession();
    void markDirty() { mSaveDirty = true; }
    ilo::SaveData gatherSave() const;
    void applySave(const ilo::SaveData &s);
    void updateBeacons();
    void pinSeedStar();
    void clearWeave();
    void completeWeave();
    void updateConstellations();
    void seedDemoConstellation();
    void resetGame();
    void addDeer(Player &deer, float x, float z, int herd = 0, float yawOffset = 0.0f);

    void setvSync(bool vSyncStatus);
    void setMouseSensitivity(float _sensitivity);
    void setScrollSensitivity(float _sensitivity);
    void setSSAA(float _SSAAamount);
    void setFOV(float _fov);

    void setCamera(Camera &c);
    void addNPC(Player &npc);
};
