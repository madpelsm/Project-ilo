#include "Window.h"

#include "Props.h"
#include "Screenshot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
// "The vale remembers" on the web: the save blob lives in localStorage. Synchronous,
// main-thread only, wrapped so private-mode / quota failures fail silently.
EM_JS(void, ilo_save_blob, (const char *s), { try { localStorage.setItem("lumenmere.save", UTF8ToString(s)); } catch (e) {} });
EM_JS(void, ilo_load_blob, (char *out, int max), {
    try { stringToUTF8(localStorage.getItem("lumenmere.save") || "", out, max); } catch (e) { stringToUTF8("", out, max); }
});
#else
#include <sys/stat.h>
#include <thread>
#endif

namespace {
// Movement tuning (metres / second), from the design spec.
const float WALK_SPEED = 4.0f;
const float SPRINT_SPEED = 6.5f;
const float FLY_VERT_SPEED = 3.0f;
const float WORLD_BOUND = 880.0f; // soft clamp near the caldera rim
const float DT_CLAMP = 0.05f;
const float COLLECT_RADIUS = 1.8f;

// Fuel / warmth tuning (warmth units), from the design spec.
const float FUEL_MAX = 100.0f;
const float FUEL_START = 70.0f;
const float FLARE_COST = 8.0f;
const float FLARE_DURATION = 1.0f;
const float FLARE_COOLDOWN = 1.5f;
const float FLARE_MULT = 2.0f;
const float DEER_SPEED = 1.0f;

// Phase 9 — falling-leaf glide (a velocity model over the height-above-ground mEyeOffset).
const float GLIDE_GRAVITY = 2.6f;  // gentle downward accel when airborne, hands off
const float TERMINAL_FALL = 4.0f;  // capped descent — luxurious, never a plummet
const float GROUND_EPS = 0.05f;    // above this height-above-ground we are "airborne"
const float GLIDE_SPEED = 7.5f;    // airborne horizontal cruise (faster than a walk)
const float AIR_ACCEL = 2.2f;      // how quickly the glide drift eases toward intent
const float AIR_DRAG = 0.5f;       // coast + decay when you let go of the sticks

// Phase 9 — deer trust / curiosity / follow (no-fail, forgiving).
const float RUSH_SPEED = 5.2f;       // player speed that reads as "rushing"
const float FLEE_RADIUS = 14.0f;
const float FLEE_TIME = 3.0f;
const float FLEE_TRUST_DROP = 0.35f; // partial, never instant 0
const float FLEE_SPEED = 4.2f;       // a brisk trot, not a panic gallop
const float TRUST_RADIUS = 12.0f;
const float CALM_SPEED = 2.2f;
const float TRUST_GAIN = 0.5f;
const float NOTICE_RADIUS = 28.0f;
const float TRUST_DECAY = 0.02f;     // a befriended deer stays friendly for minutes
const float CURIOUS_THRESH = 0.35f;
const float FOLLOW_THRESH = 0.75f;
const float CURIOUS_GAP = 6.0f;
const float FOLLOW_GAP = 3.5f;
const float FOLLOW_SPEED = 3.2f;     // keeps up with a walk, not a sprint
const float DEER_WATER_MIN = 0.8f;   // deer never step onto the mirror-Mere

// Phase 9 — wind-rivers (invisible lift+carry lanes you glide into, marked by motes).
const float RIVER_RADIUS = 12.0f;
const float RIVER_LIFT = 3.4f;       // updraft > gravity, so you rise/hold in a lane
const float RIVER_SPEED = 18.0f;     // along-lane carry
const float RIVER_TAU = 1.2f;        // easing time for the carry (no snap)

// Phase 10 — Rest, Memory & The Long Dawn.
const float LONG_DAWN_THRESHOLD = 0.999f; // radiance at which the climax breaks
const float LONG_DAWN_DURATION = 10.0f;   // length of the unison flare
const float LONG_DAWN_PEAK = 1.6f;        // apex of the global bloom term
const float LONG_DAWN_FLOOR = 0.16f;      // permanent radiant glow it settles to
const float DAWN_PHASE = 0.23f;           // the eternal golden dawn it holds
const float LONG_DAWN_AURORA_SURGE = 0.9f;
const float REST_EYE_DROP = 0.6f;
const float REST_EASE = 3.0f;
const float SCRUB_RATE = 0.10f;           // day-phase / sec while scrubbing in Rest
const float PHOTO_FLASH = 0.10f;
const float PHOTO_NOTE = 2.0f;
const float AUTOSAVE_INTERVAL = 10.0f;
const float SEED_PLANT_RADIUS = 6.0f;
const float WAKE_FROZEN_RADIUS = 70.0f;   // loaded beacons read bloomed without re-sweeping

// Calm fixed palette for woven constellations (indexed by completed count -> stable demo).
glm::vec3 weavePalette(size_t i) {
    static const glm::vec3 pal[4] = {
        glm::vec3(0.21f, 1.00f, 0.76f), // jade
        glm::vec3(1.00f, 0.78f, 0.35f), // gold
        glm::vec3(0.62f, 0.40f, 1.00f), // violet
        glm::vec3(1.00f, 0.42f, 0.62f), // rose
    };
    return pal[i & 3];
}

void buildProgram(ShaderProgram &prog, const char *vsPath, const char *fsPath) {
    Shader vs, fs;
    vs.loadShader(vsPath, GL_VERTEX_SHADER);
    fs.loadShader(fsPath, GL_FRAGMENT_SHADER);
    prog.createProgram();
    prog.attachShaderToProgram(&vs);
    prog.attachShaderToProgram(&fs);
    prog.linkProgram();
    vs.deleteShader();
    fs.deleteShader();
}
} // namespace

Window::Window() {
    mWidth = 800;
    mHeight = 600;
    mTitle = "untitled";
}

Window::Window(int width, int height, std::string title) {
    mWidth = width;
    mHeight = height;
    mTitle = title;
    std::cout << "Window with parameters set" << std::endl;
    init();
}

Window::~Window() {
    sdlDie();
}

void Window::sdlDie() {
    if (mDestroyed)
        return;
    mDestroyed = true;
    closed = true;
    std::cout << "killing SDL" << std::endl;
    // The Player objects are owned (and deleted) by main while the context is still
    // alive, so their destructors free their own GL resources; don't touch them here.
    mFireflies.destroy();
    mMushrooms.destroy();
    mBirds.destroy();
    mButterflies.destroy();
    mBeaconField.destroy();
    mTwinField.destroy();
    mWindMotes.destroy();
    mSeedField.destroy();
    mSeedPatchField.destroy();
    for (auto &f : mFields)
        f.destroy();
    mGrass.destroy();
    mTerrain.destroy();
    mHud.destroy();
    destroyFramebuffers();
    tri.destroy();
    lightUBO.destroy();
    if (mBlackTex)
        glDeleteTextures(1, &mBlackTex);
    compositeProg.deleteProgram();
    lightingProg.deleteProgram();
    geometryProg.deleteProgram();
    brightProg.deleteProgram();
    blurProg.deleteProgram();
    skyProg.deleteProgram();
    waterProg.deleteProgram();
    godrayProg.deleteProgram();
    particleProg.deleteProgram();
    ssaoProg.deleteProgram();
    ssaoBlurProg.deleteProgram();
    shadowProg.deleteProgram();
    if (mWaterVao)
        glDeleteVertexArrays(1, &mWaterVao);
    glDeleteBuffers(1, &mWaterVbo);
    if (mParticleVao)
        glDeleteVertexArrays(1, &mParticleVao);
    glDeleteBuffers(1, &mParticleVbo);
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    SDL_DestroyWindow(mSDLwindow);
    SDL_Quit();
}

void Window::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "failed to intialise video" << std::endl;
    }
#ifdef __EMSCRIPTEN__
    // WebGL2 (OpenGL ES 3.0); the WebGL version is forced by the linker flags.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 winFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifndef __EMSCRIPTEN__
    winFlags |= SDL_WINDOW_ALLOW_HIGHDPI; // on web, keep the backing store at 1:1 for perf
#endif
    mSDLwindow = SDL_CreateWindow(
        mTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mWidth, mHeight, winFlags);
    if (mSDLwindow == nullptr) {
        std::cout << "failed to create Window" << std::endl;
    }
    // Only grab the mouse for interactive play (not in the headless screenshot path).
    if (!std::getenv("ILO_SHOT"))
        SDL_SetRelativeMouseMode(SDL_TRUE);
    glContext = SDL_GL_CreateContext(mSDLwindow);
    SDL_GL_SetSwapInterval(vSync ? 1 : 0);
    windowInitialised = true;

    if (std::getenv("ILO_LOWSPEC"))
        mLowSpec = true;

    initGL();
    SDL_GL_GetDrawableSize(mSDLwindow, &mWidth, &mHeight);
    createFramebuffers();
    std::cout << "Window initialised correctly" << std::endl;
}

void Window::initGL() {
#ifdef __EMSCRIPTEN__
    // No loader on the web; enable float render targets (RGBA16F G-buffer + HDR).
    emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(), "EXT_color_buffer_float");
#else
    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        printf("Something went wrong!\n");
        exit(-1);
    }
    printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
#endif

    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);

    buildProgram(geometryProg, "./shaders/firstPassVertex.vert", "./shaders/firstPassFragment.frag");
    buildProgram(lightingProg, "./shaders/fullscreen.vert", "./shaders/secondPassFrag.frag");
    buildProgram(brightProg, "./shaders/fullscreen.vert", "./shaders/bloomBright.frag");
    buildProgram(blurProg, "./shaders/fullscreen.vert", "./shaders/bloomBlur.frag");
    buildProgram(compositeProg, "./shaders/fullscreen.vert", "./shaders/thirdPassFrag.frag");
    buildProgram(skyProg, "./shaders/fullscreen.vert", "./shaders/sky.frag");
    buildProgram(waterProg, "./shaders/water.vert", "./shaders/water.frag");
    buildProgram(godrayProg, "./shaders/fullscreen.vert", "./shaders/godray.frag");
    buildProgram(particleProg, "./shaders/particles.vert", "./shaders/particles.frag");
    buildProgram(ssaoProg, "./shaders/fullscreen.vert", "./shaders/ssao.frag");
    buildProgram(ssaoBlurProg, "./shaders/fullscreen.vert", "./shaders/ssaoBlur.frag");
    buildProgram(shadowProg, "./shaders/shadowDepth.vert", "./shaders/shadowDepth.frag");

    // Static sampler bindings.
    lightingProg.useProgram();
    GLuint lp = lightingProg.getProgramID();
    glUniform1i(glGetUniformLocation(lp, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lp, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lp, "gAlbedo"), 2);
    glUniform1i(glGetUniformLocation(lp, "gMtlProps"), 3);
    glUniform1i(glGetUniformLocation(lp, "uSkyTex"), 4);
    glUniform1i(glGetUniformLocation(lp, "uAO"), 5);
    glUniform1i(glGetUniformLocation(lp, "uShadowMap"), 6);
    glUniformBlockBinding(lp, glGetUniformBlockIndex(lp, "LightBlock"), 0);

    // SSAO: hemisphere kernel (clustered toward the surface) + a 4x4 rotation tile,
    // generated once and held in the program as constant uniform arrays.
    {
        unsigned int s = 0xA0A0BEEFu;
        auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / (float)0x1000000; };
        mSsaoKernel.clear();
        for (int i = 0; i < 16; i++) {
            glm::vec3 v(rnd() * 2.0f - 1.0f, rnd() * 2.0f - 1.0f, rnd()); // hemisphere +z
            v = glm::normalize(v) * rnd();
            float t = i / 16.0f;
            v *= (0.1f + 0.9f * t * t); // pack more samples near the origin
            mSsaoKernel.push_back(v);
        }
        mSsaoNoise.clear();
        for (int i = 0; i < 16; i++)
            mSsaoNoise.push_back(glm::vec3(rnd() * 2.0f - 1.0f, rnd() * 2.0f - 1.0f, 0.0f));

        ssaoProg.useProgram();
        GLuint sp = ssaoProg.getProgramID();
        glUniform1i(glGetUniformLocation(sp, "gPosition"), 0);
        glUniform1i(glGetUniformLocation(sp, "gNormal"), 1);
        glUniform3fv(glGetUniformLocation(sp, "uKernel"), 16, &mSsaoKernel[0].x);
        glUniform3fv(glGetUniformLocation(sp, "uNoise"), 16, &mSsaoNoise[0].x);
        glUniform1f(glGetUniformLocation(sp, "uRadius"), 1.4f);
        glUniform1f(glGetUniformLocation(sp, "uBias"), 0.04f);
        glUniform1f(glGetUniformLocation(sp, "uPower"), 1.7f);
        ssaoBlurProg.useProgram();
        glUniform1i(glGetUniformLocation(ssaoBlurProg.getProgramID(), "uAO"), 0);
    }

    brightProg.useProgram();
    glUniform1i(glGetUniformLocation(brightProg.getProgramID(), "uScene"), 0);
    blurProg.useProgram();
    glUniform1i(glGetUniformLocation(blurProg.getProgramID(), "image"), 0);
    compositeProg.useProgram();
    glUniform1i(glGetUniformLocation(compositeProg.getProgramID(), "uScene"), 0);
    glUniform1i(glGetUniformLocation(compositeProg.getProgramID(), "uBloom"), 1);
    glUniform1i(glGetUniformLocation(compositeProg.getProgramID(), "uGodray"), 2);
    godrayProg.useProgram();
    glUniform1i(glGetUniformLocation(godrayProg.getProgramID(), "uHdr"), 0);
    glUniform1i(glGetUniformLocation(godrayProg.getProgramID(), "gNormal"), 1);
    waterProg.useProgram();
    glUniform1i(glGetUniformLocation(waterProg.getProgramID(), "gPosition"), 0);

    // The Mere: a flat water plane (y=0) over the central basin.
    {
        const float E = 240.0f;
        float quad[] = {-E, 0, -E, E, 0, -E, E, 0, E, -E, 0, -E, E, 0, E, -E, 0, E};
        glGenVertexArrays(1, &mWaterVao);
        glBindVertexArray(mWaterVao);
        glGenBuffers(1, &mWaterVbo);
        glBindBuffer(GL_ARRAY_BUFFER, mWaterVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        glBindVertexArray(0);
    }

    tri.init();
    lightUBO.init();
    mHud.init();

    // Atmosphere motes: a static buffer of random seeds animated entirely in the VS.
    {
        mParticleCount = 3500;
        std::vector<glm::vec4> seeds(mParticleCount);
        unsigned int s = 0x1234abcdu;
        auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / (float)0x1000000; };
        for (auto &v : seeds)
            v = glm::vec4(rnd(), rnd(), rnd(), rnd());
        glGenVertexArrays(1, &mParticleVao);
        glBindVertexArray(mParticleVao);
        glGenBuffers(1, &mParticleVbo);
        glBindBuffer(GL_ARRAY_BUFFER, mParticleVbo);
        glBufferData(GL_ARRAY_BUFFER, seeds.size() * sizeof(glm::vec4), seeds.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
        glBindVertexArray(0);
    }

    // 1x1 black texture used as the bloom input until the bloom pass exists.
    glGenTextures(1, &mBlackTex);
    glBindTexture(GL_TEXTURE_2D, mBlackTex);
    const unsigned char black[4] = {0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapWindow(mSDLwindow);
}

void Window::createFramebuffers() {
    // G-buffer: position+emissive & normal as float, albedo & material as RGBA8, depth24.
    gBuffer.create(mWidth, mHeight);
    gBuffer.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT);          // RT0 pos.xyz, emissive in .a
    gBuffer.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT);          // RT1 normal
    gBuffer.addColor(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);    // RT2 albedo
    gBuffer.addColor(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);    // RT3 material props
    gBuffer.addDepth(GL_DEPTH_COMPONENT24);
    gBuffer.setDrawBuffers();
    gBuffer.complete("gBuffer");

    // HDR lighting target (linear, filtered for bloom sampling). Has its own depth so
    // the scene depth can be blitted in for the water pass to test against.
    hdrFBO.create(mWidth, mHeight);
    hdrFBO.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    hdrFBO.addDepth(GL_DEPTH_COMPONENT24);
    hdrFBO.setDrawBuffers();
    hdrFBO.complete("hdrFBO");

    // Bloom ping-pong targets at reduced resolution.
    int div = mLowSpec ? 4 : 2;
    int bw = std::max(1, (mWidth + div - 1) / div);
    int bh = std::max(1, (mHeight + div - 1) / div);
    bloomA.create(bw, bh);
    bloomA.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    bloomA.setDrawBuffers();
    bloomA.complete("bloomA");
    bloomB.create(bw, bh);
    bloomB.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    bloomB.setDrawBuffers();
    bloomB.complete("bloomB");
    mBloomReady = true;

    // Half-resolution procedural sky (sampled by the lighting + water passes).
    int sw = std::max(1, (mWidth + 1) / 2);
    int sh = std::max(1, (mHeight + 1) / 2);
    skyFBO.create(sw, sh);
    skyFBO.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    skyFBO.setDrawBuffers();
    skyFBO.complete("skyFBO");

    // Quarter-resolution god-ray buffer.
    int gw = std::max(1, (mWidth + 3) / 4), gh = std::max(1, (mHeight + 3) / 4);
    godrayFBO.create(gw, gh);
    godrayFBO.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    godrayFBO.setDrawBuffers();
    godrayFBO.complete("godrayFBO");

    // Half-resolution ambient-occlusion buffers (single channel, smoothed by a blur).
    int aw = std::max(1, (mWidth + 1) / 2), ah = std::max(1, (mHeight + 1) / 2);
    ssaoFBO.create(aw, ah);
    ssaoFBO.addColor(GL_R8, GL_RED, GL_UNSIGNED_BYTE, GL_LINEAR);
    ssaoFBO.setDrawBuffers();
    ssaoFBO.complete("ssaoFBO");
    ssaoBlurFBO.create(aw, ah);
    ssaoBlurFBO.addColor(GL_R8, GL_RED, GL_UNSIGNED_BYTE, GL_LINEAR);
    ssaoBlurFBO.setDrawBuffers();
    ssaoBlurFBO.complete("ssaoBlurFBO");

    // Sun shadow map: a fixed-size samplable depth texture (independent of window size).
#ifdef __EMSCRIPTEN__
    mShadowRes = 1024;
#else
    mShadowRes = mLowSpec ? 1024 : 2048;
#endif
    shadowFBO.create(mShadowRes, mShadowRes);
    shadowFBO.addDepthTexture(GL_DEPTH_COMPONENT24);
    shadowFBO.complete("shadowFBO");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::destroyFramebuffers() {
    gBuffer.destroy();
    hdrFBO.destroy();
    bloomA.destroy();
    bloomB.destroy();
    skyFBO.destroy();
    godrayFBO.destroy();
    ssaoFBO.destroy();
    ssaoBlurFBO.destroy();
    shadowFBO.destroy();
    mBloomReady = false;
}

void Window::loadGeometries() {
#ifdef __EMSCRIPTEN__
    // Single-threaded on the web (no pthreads / SharedArrayBuffer needed).
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->loadDefaultGeometry();
    }
#else
    std::vector<std::thread> loaders;
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        loaders.push_back(std::thread(&Player::loadDefaultGeometry, mGameObjects[i]));
    }
    for (unsigned int i = 0; i < loaders.size(); i++) {
        loaders[i].join();
    }
#endif
}

void Window::initAssets() {
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->initGL();
    }
    if (mProps)
        mProps->initGL();
    mTerrain.build(896.0f, 320);
    scatterWorld();
    mFireflies.init(60);
    mMushrooms.init(10);
    mBirds.init(30);
    mButterflies.init(140);

    // Heartwood beacons scattered across the regions, waiting to be woken.
    const float ang[6] = {0.5f, 1.5f, 2.5f, 3.6f, 4.6f, 5.7f};
    const float rad[6] = {230.f, 320.f, 400.f, 270.f, 360.f, 300.f};
    for (int i = 0; i < 6; i++) {
        float x = std::cos(ang[i]) * rad[i], z = std::sin(ang[i]) * rad[i];
        mBeacons.push_back({glm::vec3(x, std::max(1.0f, ilo::terrainHeight(x, z)), z), false, 0.0f});
    }
    mBeaconField.buildDynamic(proc::makeBeacon(), 8);
    updateBeacons();

    // Fallen-twin markers for woven constellations (same glassy spire as a beacon).
    mTwinField.buildDynamic(proc::makeBeacon(), MAX_CSTARS);

    // Wind-rivers: a few invisible lift+carry lanes you can glide into, made visible by
    // streams of drifting motes. Lanes climb (y = ground + offset) so they read as rising.
    auto lane = [](float ax, float az, float ah, float bx, float bz, float bh) {
        WindRiver r;
        r.a = glm::vec3(ax, ilo::terrainHeightFast(ax, az) + ah, az);
        r.b = glm::vec3(bx, ilo::terrainHeightFast(bx, bz) + bh, bz);
        return r;
    };
    mWindRivers.push_back(lane(150.0f, 150.0f, 8.0f, 30.0f, -120.0f, 48.0f));  // Mere crossing
    mWindRivers.push_back(lane(-280.0f, 120.0f, 30.0f, 120.0f, 280.0f, 45.0f)); // ring loop
    mWindRivers.push_back(lane(260.0f, -40.0f, 12.0f, 360.0f, -260.0f, 70.0f)); // Reach updraft
    {
        proc::Mesh mote;
        proc::sphere(mote, glm::vec3(0.0f), 1.0f, 2, 6, glm::vec3(0.7f, 0.9f, 1.0f), glm::vec3(8.0f, 0.1f, 0.0f));
        mWindMotes.buildDynamic(mote, (int)mWindRivers.size() * 28);
    }
    updateWindMotes();

    // Phase 10: the seed that waits at the Mere after The Long Dawn (a glowing pod), and
    // the patch a planted seed blooms. mSeedField holds 0..1 instance; the patch up to ~180.
    mSeedField.buildDynamic(proc::makeMushroom(), 1);
    {
        proc::Rng fr(4242u);
        mSeedPatchField.buildDynamic(proc::makeFlower(fr), 200);
    }
    mSeedPos = glm::vec3(0.0f, std::max(0.5f, ilo::terrainHeightFast(0.0f, 150.0f)) + 0.3f, 150.0f);

    // Persistence — "the vale remembers." On the web it's always on (localStorage); on
    // native it persists to ILO_SAVE or ~/.lumenmere_save, but stays inert during headless
    // ILO_SHOT runs (unless ILO_SAVE is set) so screenshots never load a stale dev save.
#ifdef __EMSCRIPTEN__
    mPersistEnabled = true;
#else
    if (std::getenv("ILO_NOSAVE")) {
        mPersistEnabled = false;
    } else if (const char *sp = std::getenv("ILO_SAVE")) {
        mSavePath = sp;
        mPersistEnabled = true;
    } else if (std::getenv("ILO_SHOT")) {
        mPersistEnabled = false;
    } else if (const char *h = std::getenv("HOME")) {
        mSavePath = std::string(h) + "/.lumenmere_save";
        mPersistEnabled = true;
    }
#endif
    loadSession();
}

void Window::updateBeacons() {
    glm::vec3 cam = mCamera.mPosition;
    // Wake a sleeping beacon you wander up to: a region-wide bloom shockwave, a burst
    // of warmth, and a permanent jump in the world's radiance.
    if (mState == GameState::Playing) {
        for (Beacon &b : mBeacons) {
            if (b.lit)
                continue;
            float dx = b.pos.x - cam.x, dz = b.pos.z - cam.z;
            if (dx * dx + dz * dz < 64.0f) { // within 8m
                b.lit = true;
                b.igniteTime = mTime;
                mGrovesAwake++;
                mRadiance = std::min(1.0f, mRadiance + 0.15f);
                mFuelW = FUEL_MAX;
                mFlash = std::min(0.4f, mFlash + 0.3f);
                if (mWakeEvents.size() < 8)
                    mWakeEvents.push_back(glm::vec4(b.pos.x, b.pos.z, mTime, 0.0f));
                if (mPulses.size() < 8)
                    mPulses.push_back({glm::vec3(b.pos.x, b.pos.y + 3.0f, b.pos.z), glm::vec3(1.0f, 0.85f, 0.5f), 0.0f, 0.8f});
            }
        }
    }
    // Rebuild the (few) beacon instances: dim teal asleep, blazing gold awake.
    std::vector<FieldInstance> inst;
    inst.reserve(mBeacons.size());
    for (const Beacon &b : mBeacons) {
        FieldInstance fi;
        fi.pos = b.pos;
        if (b.lit) {
            float breathe = 4.0f + 1.0f * std::sin(6.2831f * 0.4f * mTime + b.pos.x);
            fi.tintEmissive = glm::vec4(1.0f, 0.85f, 0.5f, breathe);
        } else {
            // a soft beckoning pulse so a sleeping Heartwood reads as a place to seek
            float pulse = 0.8f + 0.35f * std::sin(6.2831f * 0.5f * mTime + b.pos.x);
            fi.tintEmissive = glm::vec4(0.4f, 0.7f, 0.95f, pulse);
        }
        fi.xform = glm::vec4(1.5f, b.pos.x * 0.7f, 0.0f, 0.0f);
        inst.push_back(fi);
    }
    mBeaconField.update(inst);
}

void Window::scatterWorld() {
    mFields.reserve(16);
    proc::Rng rng(7777u);
    auto slopeAt = [](float x, float z) {
        float e = 2.0f;
        float hl = ilo::terrainHeight(x - e, z), hr = ilo::terrainHeight(x + e, z);
        float hd = ilo::terrainHeight(x, z - e), hu = ilo::terrainHeight(x, z + e);
        glm::vec3 n = glm::normalize(glm::vec3(hl - hr, 2.0f * e, hd - hu));
        return 1.0f - n.y;
    };

    // Dense grass around the player. Placement is anchored to ABSOLUTE world cells
    // (not a camera-relative pattern) so it never swims as you move.
    mGrass.buildDynamic(proc::makeGrassTuft(rng), 16000);
    updateGrass(true);

    // Conifers on the slopes (two baked variants, gentle sway).
    for (int v = 0; v < 2; v++) {
        proc::Rng tr(100u + v);
        proc::Mesh tree;
        proc::tree(tree, glm::vec3(0), tr);
        std::vector<FieldInstance> inst;
        for (int i = 0; i < 2000 && (int)inst.size() < 450; i++) {
            float x = rng.range(-720, 720), z = rng.range(-720, 720);
            float r = std::sqrt(x * x + z * z), h = ilo::terrainHeight(x, z);
            if (h < 1.0f || r < 120.0f || r > 740.0f || slopeAt(x, z) > 0.5f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(x, h, z);
            fi.tintEmissive = glm::vec4(rng.range(0.8f, 1.15f), rng.range(0.85f, 1.1f), rng.range(0.8f, 1.1f), 0.0f);
            fi.xform = glm::vec4(rng.range(0.7f, 1.5f), rng.range(0, 6.2831853f), 0.16f, rng.range(0, 6.2831853f));
            inst.push_back(fi);
        }
        mFields.emplace_back();
        mFields.back().build(tree, inst);
    }

    // Broadleaf trees + white birches, for a varied woodland (not cloned conifers).
    {
        proc::Rng br(202u);
        proc::Mesh leaf;
        proc::broadleaf(leaf, glm::vec3(0), br);
        std::vector<FieldInstance> inst;
        for (int i = 0; i < 1600 && (int)inst.size() < 320; i++) {
            float x = rng.range(-560, 560), z = rng.range(-560, 560);
            float r = std::sqrt(x * x + z * z), h = ilo::terrainHeight(x, z);
            if (h < 1.0f || r < 130.0f || r > 560.0f || slopeAt(x, z) > 0.45f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(x, h, z);
            fi.tintEmissive = glm::vec4(rng.range(0.85f, 1.15f), rng.range(0.85f, 1.1f), rng.range(0.8f, 1.05f), 0.0f);
            fi.xform = glm::vec4(rng.range(0.8f, 1.4f), rng.range(0, 6.2831853f), 0.14f, rng.range(0, 6.2831853f));
            inst.push_back(fi);
        }
        mFields.emplace_back();
        mFields.back().build(leaf, inst);
    }
    {
        proc::Rng bk(303u);
        proc::Mesh bMesh;
        proc::birch(bMesh, glm::vec3(0), bk);
        std::vector<FieldInstance> inst;
        for (int i = 0; i < 1400 && (int)inst.size() < 220; i++) {
            float x = rng.range(-520, 520), z = rng.range(-520, 520);
            float r = std::sqrt(x * x + z * z), h = ilo::terrainHeight(x, z);
            if (h < 1.0f || r < 150.0f || r > 520.0f || slopeAt(x, z) > 0.4f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(x, h, z);
            fi.tintEmissive = glm::vec4(rng.range(0.9f, 1.1f), rng.range(0.9f, 1.1f), rng.range(0.9f, 1.1f), 0.0f);
            fi.xform = glm::vec4(rng.range(0.8f, 1.3f), rng.range(0, 6.2831853f), 0.22f, rng.range(0, 6.2831853f));
            inst.push_back(fi);
        }
        mFields.emplace_back();
        mFields.back().build(bMesh, inst);
    }

    // Glowing wildflowers in the meadows (emissive, sway).
    {
        proc::Mesh flower = proc::makeFlower(rng);
        glm::vec3 pal[4] = {{1.0f, 0.85f, 0.45f}, {1.0f, 0.55f, 0.75f}, {0.5f, 0.8f, 1.0f}, {0.8f, 0.55f, 1.0f}};
        std::vector<FieldInstance> inst;
        for (int i = 0; i < 8000 && (int)inst.size() < 2200; i++) {
            float x = rng.range(-450, 450), z = rng.range(-450, 450);
            float h = ilo::terrainHeight(x, z);
            if (h < 0.8f || std::sqrt(x * x + z * z) > 460.0f || slopeAt(x, z) > 0.4f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(x, h, z);
            glm::vec3 c = pal[(int)(rng.f() * 3.999f) & 3];
            fi.tintEmissive = glm::vec4(c, rng.range(1.2f, 2.4f));
            fi.xform = glm::vec4(rng.range(0.8f, 1.7f), rng.range(0, 6.2831853f), 0.5f, rng.range(0, 6.2831853f));
            inst.push_back(fi);
        }
        mFields.emplace_back();
        mFields.back().build(flower, inst);
    }

    // Boulders strewn over the land (no wind).
    {
        proc::Rng rr(55u);
        proc::Mesh boulder;
        proc::rock(boulder, glm::vec3(0), 0.5f, rr, glm::vec3(0.12f, 0.12f, 0.13f));
        std::vector<FieldInstance> inst;
        for (int i = 0; i < 1600 && (int)inst.size() < 320; i++) {
            float x = rng.range(-760, 760), z = rng.range(-760, 760);
            float r = std::sqrt(x * x + z * z), h = ilo::terrainHeight(x, z);
            if (r < 60.0f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(x, h - 0.1f, z);
            float g = rng.range(0.7f, 1.2f);
            fi.tintEmissive = glm::vec4(g, g, g * 1.05f, 0.0f);
            fi.xform = glm::vec4(rng.range(0.6f, 2.4f), rng.range(0, 6.2831853f), 0.0f, 0.0f);
            inst.push_back(fi);
        }
        mFields.emplace_back();
        mFields.back().build(boulder, inst);
    }
}

void Window::updateGrass(bool force) {
    glm::vec2 cam(mCamera.mPosition.x, mCamera.mPosition.z);
    glm::vec2 cell = glm::floor(cam / 6.0f) * 6.0f;
    if (!force && cell == mGrassCenter)
        return;
    mGrassCenter = cell;

    const float R = 64.0f, s = 1.1f;
    auto hash = [](int a, int b) {
        unsigned int h = (unsigned int)(a * 73856093) ^ (unsigned int)(b * 19349663);
        h ^= h >> 13;
        h *= 0x85ebca6bu;
        h ^= h >> 16;
        return (h & 0xFFFFFFu) / (float)0x1000000;
    };
    int gx0 = (int)std::floor((cam.x - R) / s), gx1 = (int)std::floor((cam.x + R) / s);
    int gz0 = (int)std::floor((cam.y - R) / s), gz1 = (int)std::floor((cam.y + R) / s);
    std::vector<FieldInstance> inst;
    inst.reserve(16000);
    for (int gz = gz0; gz <= gz1 && (int)inst.size() < 16000; gz++) {
        for (int gx = gx0; gx <= gx1 && (int)inst.size() < 16000; gx++) {
            float h0 = hash(gx, gz), h1 = hash(gx * 3 + 1, gz), h2 = hash(gx, gz * 7 + 2), h3 = hash(gx - 5, gz * 2 + 9);
            float wx = gx * s + (h0 - 0.5f) * s, wz = gz * s + (h1 - 0.5f) * s; // jittered, but world-stable
            float dx = wx - cam.x, dz = wz - cam.y, d2 = dx * dx + dz * dz;
            if (d2 > R * R)
                continue;
            float ty = ilo::terrainHeightFast(wx, wz);
            if (ty < 0.5f)
                continue; // keep grass out of the lake
            float d = std::sqrt(d2);
            float fade = 1.0f - std::max(0.0f, (d - R * 0.55f) / (R * 0.45f));
            if (fade <= 0.02f)
                continue;
            FieldInstance fi;
            fi.pos = glm::vec3(wx, ty, wz);
            fi.tintEmissive = glm::vec4(0.8f + 0.3f * h2, 0.9f + 0.3f * h3, 0.78f + 0.22f * h0, 0.0f);
            fi.xform = glm::vec4((0.7f + 0.95f * h3) * fade, h2 * 6.2831853f, 0.9f, h0 * 6.2831853f);
            inst.push_back(fi);
        }
    }
    mGrass.update(inst);
}

void Window::stepFrameWeb() {
    double now = SDL_GetTicks() / 1000.0;
    mDt = (float)std::min((double)DT_CLAMP, std::max(0.0, now - mPrevSeconds));
    mPrevSeconds = now;
    mTime += mDt;
    checkEvents();
    update();
    render();
}

#ifdef __EMSCRIPTEN__
static void ilo_em_loop(void *arg) { static_cast<Window *>(arg)->stepFrameWeb(); }
#endif

void Window::run() {
    if (!windowInitialised) {
        std::cout << "Window not initialised" << std::endl;
        return;
    }

#ifdef __EMSCRIPTEN__
    // The browser owns the event loop; drive one frame per animation tick.
    mPrevSeconds = SDL_GetTicks() / 1000.0;
    emscripten_set_main_loop_arg(ilo_em_loop, this, 0, 1);
    return;
#else
    // Headless verification harness (see Screenshot.h).
    const char *shotPath = std::getenv("ILO_SHOT");
    const char *shotFrameEnv = std::getenv("ILO_SHOT_FRAME");
    const char *camEnv = std::getenv("ILO_CAM");
    int shotFrame = shotFrameEnv ? std::atoi(shotFrameEnv) : 90;
    int frame = 0;
    if (const char *fuelEnv = std::getenv("ILO_FUEL")) {
        // testing knob: preview the lantern at a given warmth, held constant
        mFuelW = std::max(0.0f, std::min(1.0f, (float)std::atof(fuelEnv))) * FUEL_MAX;
        mFreezeFuel = true;
        mState = GameState::Playing;
    }
    if (const char *cEnv = std::getenv("ILO_COLLECTED"))
        mCollected = std::atoi(cEnv); // testing knob: preview Heart progress / win state
    if (const char *dEnv = std::getenv("ILO_DAYPHASE")) {
        mDayPhase = (float)std::atof(dEnv); // testing knob: hold a fixed time of day
        mFreezeDay = true;
    }
    if (std::getenv("ILO_FLY"))
        mFreeCam = true; // aerial screenshots: use the raw ILO_CAM height (no ground-follow)
    if (shotPath && camEnv) {
        float x, y, z, yaw, pitch;
        if (std::sscanf(camEnv, "%f,%f,%f,%f,%f", &x, &y, &z, &yaw, &pitch) == 5) {
            mCamera.mPosition = glm::vec3(x, y, z);
            mCamera.setYawPitch(yaw, pitch);
        }
    }
    if (std::getenv("ILO_DEMO_SKY")) // verify Phase 8: a woven sky in front of the demo camera
        seedDemoConstellation();
    if (const char *de = std::getenv("ILO_DEMO_DEER")) { // verify Phase 9 inhabitants
        mDemoDeer = std::atoi(de);
        mState = GameState::Playing;
        if (!mDeer.empty()) {
            glm::vec3 look = glm::normalize(mCamera.mLookDir);
            glm::vec2 fwd(look.x, look.z);
            if (glm::length(fwd) > 1e-4f)
                fwd = glm::normalize(fwd);
            DeerAgent &d = mDeer[0]; // a companion placed just ahead of the demo camera
            d.x = mCamera.mPosition.x + fwd.x * 4.5f;
            d.z = mCamera.mPosition.z + fwd.y * 4.5f;
            d.tx = d.x;
            d.tz = d.z;
            d.trust = (mDemoDeer == 2) ? 0.95f : 1.0f; // rush test starts trusting, then flees
            d.yaw = std::atan2(mCamera.mPosition.x - d.x, mCamera.mPosition.z - d.z) + d.yawOffset;
        }
    }
    if (std::getenv("ILO_DEMO_GLIDE")) // verify Phase 9 falling-leaf glide
        mDemoGlide = true;
    // Phase 10 verification hooks.
    if (const char *re = std::getenv("ILO_RADIANCE"))
        mRadiance = std::max(0.0f, std::min(1.0f, (float)std::atof(re)));
    if (const char *ld = std::getenv("ILO_LONGDAWN")) {
        mState = GameState::Playing;
        std::string v = ld;
        if (v == "settled") {
            mLongDawn = Dawn::Settled;
            mBloom = LONG_DAWN_FLOOR;
            mRadiance = 1.0f;
            mSeedPresent = true;
            if (!mFreezeDay)
                mDayPhase = DAWN_PHASE;
        } else { // "flare"
            mLongDawn = Dawn::Flaring;
            mDawnT = std::getenv("ILO_DAWN_T") ? (float)std::atof(std::getenv("ILO_DAWN_T")) : 5.0f;
        }
    }
    if (std::getenv("ILO_REST")) {
        mState = GameState::Playing;
        enterRest();
    }
    if (std::getenv("ILO_JOURNAL"))
        captureJournal();
    if (std::getenv("ILO_NOSHADOW"))
        mNoShadow = true; // A/B: force the sun shadow off
    if (const char *sd = std::getenv("ILO_SHADOWDEBUG"))
        mShadowDebug = std::atoi(sd); // 1 = grayscale shadow factor

    mPrevSeconds = SDL_GetTicks() / 1000.0;
    while (!closed) {
        double now = SDL_GetTicks() / 1000.0;
        mDt = (float)std::min((double)DT_CLAMP, std::max(0.0, now - mPrevSeconds));
        mPrevSeconds = now;
        mTime += mDt;

        if (!shotPath)
            checkEvents();
        update();

        bool capture = (shotPath && ++frame >= shotFrame);
        if (capture) {
            mPendingShot = true; // render() reads the back buffer before SwapWindow
            mShotPath = shotPath;
        }
        render();
        if (capture) {
            std::cout << "Saved screenshot to " << shotPath << " (" << mWidth << "x" << mHeight << ")" << std::endl;
            saveSession(); // persist the (possibly forced) state so a reload test can read it
            break;
        }
    }
#endif
}

void Window::checkEvents() {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    // While resting you sit still and own the clock: scrub the day with the arrows.
    mScrubDir = mResting ? ((state[SDL_SCANCODE_RIGHT] ? 1 : 0) - (state[SDL_SCANCODE_LEFT] ? 1 : 0)) : 0;
    bool canMove = (mState == GameState::Playing || mState == GameState::Intro) && !mResting;
    bool sprint = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    bool moving = state[SDL_SCANCODE_W] || state[SDL_SCANCODE_S] || state[SDL_SCANCODE_A] || state[SDL_SCANCODE_D];
    mSprinting = canMove && sprint && moving;
    mMoving = canMove && moving;
    float speed = (sprint ? SPRINT_SPEED : WALK_SPEED) * mDt;
    // Vertical intent is recorded as latches; the falling-leaf integration happens in
    // updateGlide() (so the headless loop, which skips checkEvents, can drive it too).
    mAscendInput = canMove && state[SDL_SCANCODE_SPACE];
    mDescendInput = canMove && (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_C]);
    if (canMove) {
        if (!mAirborne) {
            // Grounded: the tranquil default, direct walk/strafe (unchanged).
            if (state[SDL_SCANCODE_W])
                mCamera.moveForward(speed);
            if (state[SDL_SCANCODE_S])
                mCamera.moveForward(-speed);
            if (state[SDL_SCANCODE_A])
                mCamera.moveRight(-speed);
            if (state[SDL_SCANCODE_D])
                mCamera.moveRight(speed);
        } else {
            // Airborne: steer the glide — accumulate drift toward the wish direction
            // (translation itself is applied in updateGlide so it also runs headless).
            glm::vec3 f = mCamera.mLookDir;
            glm::vec2 fwd(f.x, f.z);
            if (glm::length(fwd) > 1e-4f)
                fwd = glm::normalize(fwd);
            glm::vec2 right(fwd.y, -fwd.x); // 90° in XZ
            glm::vec2 wish(0.0f);
            if (state[SDL_SCANCODE_W]) wish += fwd;
            if (state[SDL_SCANCODE_S]) wish -= fwd;
            if (state[SDL_SCANCODE_D]) wish += right;
            if (state[SDL_SCANCODE_A]) wish -= right;
            if (glm::length(wish) > 1e-4f) {
                wish = glm::normalize(wish) * (sprint ? GLIDE_SPEED * 1.15f : GLIDE_SPEED);
                mGlideVel += (wish - mGlideVel) * std::min(1.0f, AIR_ACCEL * mDt);
            } else {
                mGlideVel *= std::max(0.0f, 1.0f - AIR_DRAG * mDt); // coast like a leaf
            }
        }
    }

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEMOTION: {
            float dPitch = -mMouseSensitivity * event.motion.yrel;
            if (mInvertY)
                dPitch = -dPitch;
            mCamera.rotate(-mMouseSensitivity * event.motion.xrel, dPitch);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT && mState == GameState::Playing &&
                mFlareCooldown <= 0.0f && mFuelW >= FLARE_COST) {
                mFuelW -= FLARE_COST;
                mFlareTimer = FLARE_DURATION;
                mFlareCooldown = FLARE_COOLDOWN;
                mFlash = std::min(0.25f, mFlash + 0.1f);
            }
            // Right-click pins a seed-star at the reticle: weave the night sky.
            if (event.button.button == SDL_BUTTON_RIGHT && mState == GameState::Playing)
                pinSeedStar();
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                resize();
            break;
        case SDL_QUIT:
            saveSession(); // the vale remembers
            closed = true;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                saveSession(); // the vale remembers
                closed = true;
                break;
            case SDL_SCANCODE_P:
                if (mState == GameState::Playing)
                    mState = GameState::Paused;
                else if (mState == GameState::Paused)
                    mState = GameState::Playing;
                break;
            case SDL_SCANCODE_I:
                mInvertY = !mInvertY; // flip vertical look (trackpad preference)
                break;
            case SDL_SCANCODE_F: // finish the weave (>= 3 pins) into a constellation
                if (mState == GameState::Playing && (int)mWeavePins.size() >= 3)
                    completeWeave();
                break;
            case SDL_SCANCODE_Q: // calmly cancel the in-progress weave
                clearWeave();
                break;
            case SDL_SCANCODE_T: // Rest / Observe — sit, hide the HUD, scrub the sky
                if (mState == GameState::Playing) {
                    if (mResting)
                        exitRest();
                    else
                        enterRest();
                }
                break;
            case SDL_SCANCODE_G: // Field Journal — keep a clean keepsake photo
                captureJournal();
                break;
            case SDL_SCANCODE_E: // Plant the seed at the Mere (proximity-gated)
                plantSeed();
                break;
            case SDL_SCANCODE_R:
                resetGame();
                break;
            case SDL_SCANCODE_RETURN:
                if (event.key.keysym.mod & KMOD_LALT) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(mSDLwindow, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }
                break;
            case SDL_SCANCODE_F11:
                windowMaximised = !windowMaximised;
                if (windowMaximised)
                    SDL_MaximizeWindow(mSDLwindow);
                else
                    SDL_RestoreWindow(mSDLwindow);
                break;
            default:
                break;
            }
            break;
        }
    }

    // Soft world bound far out at the rim (the open world fills the space within).
    mCamera.mPosition.x = std::max(-WORLD_BOUND, std::min(WORLD_BOUND, mCamera.mPosition.x));
    mCamera.mPosition.z = std::max(-WORLD_BOUND, std::min(WORLD_BOUND, mCamera.mPosition.z));
    mCamera.update();
}

void Window::packLights() {
    mLights.clear();
    // Lantern is always lights[0], bound to the camera.
    glm::vec3 lp = mCamera.mPosition;
    if (mMoving) // subtle head-bob while walking
        lp.y += 0.05f * std::sin(6.2831f * 2.0f * mTime);
    float f = mFuel;
    float radius = 3.0f + 9.0f * f;
    float li = 0.6f + 2.4f * f;
    radius *= mBoonLantern; // woven constellations gently widen + warm the lantern
    li *= mBoonLantern;
    // Flare: a short, fading burst that doubles the lantern's reach.
    if (mFlareTimer > 0.0f) {
        float fr = mFlareTimer / FLARE_DURATION;
        float mult = 1.0f + (FLARE_MULT - 1.0f) * fr;
        radius *= mult;
        li *= mult;
    }
    // Low-warmth: the lantern just breathes a little (gentle, never a harsh cut-out).
    if (f < 0.25f)
        li *= 1.0f + 0.10f * std::sin(6.2831f * 1.5f * mTime);
    ilo::OmniLightGPU lantern;
    lantern.posRadius[0] = lp.x;
    lantern.posRadius[1] = lp.y;
    lantern.posRadius[2] = lp.z;
    lantern.posRadius[3] = radius;
    glm::vec3 lc(1.00f, 0.72f, 0.42f);
    lantern.colorIntensity[0] = lc.x;
    lantern.colorIntensity[1] = lc.y;
    lantern.colorIntensity[2] = lc.z;
    lantern.colorIntensity[3] = li;
    mLights.push_back(lantern);

    // Collection pulses: a bright expanding flash where each firefly was caught.
    for (const Pulse &p : mPulses) {
        float t = p.age / p.life; // 0..1
        ilo::OmniLightGPU pl;
        pl.posRadius[0] = p.pos.x;
        pl.posRadius[1] = p.pos.y;
        pl.posRadius[2] = p.pos.z;
        pl.posRadius[3] = 1.0f + 4.0f * t;
        pl.colorIntensity[0] = p.color.x;
        pl.colorIntensity[1] = p.color.y;
        pl.colorIntensity[2] = p.color.z;
        pl.colorIntensity[3] = (1.0f - t) * 8.0f;
        mLights.push_back(pl);
    }

    // Heart of the Grove: a dedicated light that brightens as the player progresses.
    {
        float p = mHeartP;
        ilo::OmniLightGPU heart;
        heart.posRadius[0] = mHeartPos.x;
        heart.posRadius[1] = mHeartPos.y;
        heart.posRadius[2] = mHeartPos.z;
        heart.posRadius[3] = glm::mix(4.0f, 20.0f, p);
        float intensity = glm::mix(0.3f, 4.0f, p);
        if (mLongDawn != Dawn::None) // the Wellheart blazes through The Long Dawn
            intensity = 5.0f + 0.6f * std::sin(6.2831f * 0.5f * mTime) + 0.5f * mBloom;
        heart.colorIntensity[0] = mHeartColor.x;
        heart.colorIntensity[1] = mHeartColor.y;
        heart.colorIntensity[2] = mHeartColor.z;
        heart.colorIntensity[3] = intensity;
        mLights.push_back(heart);
    }

    // Woken Heartwood beacons: a tall warm light column each.
    for (const Beacon &b : mBeacons) {
        if (!b.lit)
            continue;
        ilo::OmniLightGPU L;
        L.posRadius[0] = b.pos.x;
        L.posRadius[1] = b.pos.y + 4.0f;
        L.posRadius[2] = b.pos.z;
        L.posRadius[3] = 32.0f;
        float in = 3.5f + 0.8f * std::sin(6.2831f * 0.4f * mTime + b.pos.x);
        L.colorIntensity[0] = 1.0f;
        L.colorIntensity[1] = 0.8f;
        L.colorIntensity[2] = 0.5f;
        L.colorIntensity[3] = in;
        mLights.push_back(L);
    }

    // Firefly + mushroom lights (nearest to the camera), capped to keep the loop cheap.
    mFireflies.appendLights(mLights, mTime, mCamera.mPosition, mLowSpec ? 12 : 28);
    mMushrooms.appendLights(mLights, mTime, mCamera.mPosition, mLowSpec ? 4 : 6);

    // Floating origin: lights are built in world space; shift them into the same
    // origin-relative space the G-buffer positions are stored in.
    for (auto &L : mLights) {
        L.posRadius[0] -= mRenderOrigin.x;
        L.posRadius[1] -= mRenderOrigin.y;
        L.posRadius[2] -= mRenderOrigin.z;
    }

    lightUBO.upload(mLights.data(), (int)mLights.size());
}

void Window::update() {
    // Falling-leaf glide: integrate the vertical velocity into mEyeOffset and carry the
    // airborne horizontal drift (also lifts/sweeps you when inside a wind-river).
    updateGlide();

    // Ground-follow: ride the terrain at eye height (raise mEyeOffset to fly up).
    if (!mFreeCam) {
        float g = ilo::terrainHeightFast(mCamera.mPosition.x, mCamera.mPosition.z) + mEyeOffset;
        // Rest/Observe seats you a little lower (eased), without fighting the glide floor.
        float restTarget = mResting ? REST_EYE_DROP : 0.0f;
        mRestDrop += (restTarget - mRestDrop) * std::min(1.0f, REST_EASE * mDt);
        mCamera.mPosition.y = g - mRestDrop;
        mCamera.update();
    }

    // Player horizontal speed (deer read this as calm vs rushing). Guard the first frame.
    glm::vec2 cxz(mCamera.mPosition.x, mCamera.mPosition.z);
    mPlayerSpeed = mPrevCamInit ? glm::length(cxz - mPrevCamXZ) / std::max(mDt, 1e-4f) : 0.0f;
    mPrevCamXZ = cxz;
    mPrevCamInit = true;

    // Floating origin: snap to a 128m grid near the camera (only the XZ plane).
    mRenderOrigin = glm::vec3(std::round(mCamera.mPosition.x / 128.0f) * 128.0f, 0.0f,
                              std::round(mCamera.mPosition.z / 128.0f) * 128.0f);

    updateGrass(false); // recentre the dense grass disc when the player crosses a cell

    if (mState == GameState::Intro) {
        mIntroTimer -= mDt;
        if (mIntroTimer <= 0.0f)
            mState = GameState::Playing;
    }

    if (mState == GameState::Playing) {
        float fuelGained = 0.0f;
        std::vector<CollectEvent> events;
        int got = mFireflies.update(mDt, mTime, mCamera.mPosition, COLLECT_RADIUS, fuelGained, &events);
        mCollected += got;
        mFuelW = std::min(FUEL_MAX, mFuelW + fuelGained);
        for (const CollectEvent &e : events) {
            // combo: chaining catches within the window stacks a warmth bonus
            mCombo = (mComboTimer > 0.0f) ? mCombo + 1 : 1;
            mComboTimer = 3.0f;
            mFuelW = std::min(FUEL_MAX, mFuelW + (mCombo - 1) * 1.5f);
            mRadiance = std::min(1.0f, mRadiance + 0.012f); // the vale grows brighter
            if (mPulses.size() < 8)
                mPulses.push_back({e.pos, e.color, 0.0f, 0.35f});
            mFlash = std::min(0.25f, mFlash + 0.15f);
        }
        // glowing mushrooms: fly close to harvest a little warmth
        float shroomWarmth = mMushrooms.update(mDt, mTime, mCamera.mPosition, 2.0f);
        if (shroomWarmth > 0.0f) {
            mFuelW = std::min(FUEL_MAX, mFuelW + shroomWarmth);
            mRadiance = std::min(1.0f, mRadiance + shroomWarmth * 0.0006f);
            mFlash = std::min(0.25f, mFlash + 0.12f);
        }

        // Tranquil warmth: NO fail state. Near light & life (fireflies, mushrooms, the
        // Heart) the lantern is kept topped up, so roaming the living world is free.
        // Only the genuine dark wilds dim it gently — and at zero it just shrinks to a
        // small personal halo, never ends the game. This rewards wandering toward light.
        if (!mFreezeFuel) {
            glm::vec3 cam = mCamera.mPosition;
            float lifeD = std::min(mFireflies.nearestDist(cam), mMushrooms.nearestDist(cam));
            lifeD = std::min(lifeD, glm::length(cam - mHeartPos));
            if (lifeD < 16.0f)
                mFuelW = std::min(FUEL_MAX, mFuelW + 10.0f * mDt); // bask in radiance
            else
                mFuelW = std::max(0.0f, mFuelW - 1.0f * mDt); // gentle dim in the dark
        }

        // No win/lose screen: the firefly count only feeds Radiance now. The single
        // ending is The Long Dawn — earned at full Radiance, and it never leaves play.
    }
    updateLongDawn();

    // advance feedback timers
    mFlash = std::max(0.0f, mFlash - mDt * 1.5f);
    if (mFlareTimer > 0.0f)
        mFlareTimer -= mDt;
    if (mFlareCooldown > 0.0f)
        mFlareCooldown -= mDt;
    if (mComboTimer > 0.0f) {
        mComboTimer -= mDt;
        if (mComboTimer <= 0.0f)
            mCombo = 0;
    }
    // The clock: while resting you OWN it (scrub with the arrows); otherwise it drifts,
    // and once The Long Dawn breaks it eases to — and holds — an eternal golden dawn.
    if (mResting) {
        mDayPhase += mScrubDir * SCRUB_RATE * mDt;
        mDayPhase -= std::floor(mDayPhase);
    } else if (!mFreezeDay) {
        mDayPhase += mDt / mDayLength;
        if (mLongDawn != Dawn::None)
            mDayPhase += (DAWN_PHASE - mDayPhase) * std::min(1.0f, mDt * 0.5f);
        mDayPhase -= std::floor(mDayPhase); // wrap to [0,1)
    }
    mSky.update(mDayPhase, mRadiance);
    mBirds.update(mDt, mTime, mSky.nightAmount);
    mButterflies.update(mDt, mTime, mSky.nightAmount);

    // Fish rises on the Mere: spawn an occasional expanding ripple ring out over the
    // open water, and retire the ones that have faded. Keeps the lake from reading dead.
    auto drng = [&]() {
        mDimpleRng ^= mDimpleRng << 13;
        mDimpleRng ^= mDimpleRng >> 17;
        mDimpleRng ^= mDimpleRng << 5;
        return (mDimpleRng & 0xFFFFFF) / (float)0x1000000;
    };
    for (size_t i = 0; i < mDimples.size();) {
        if (mTime - mDimples[i].z > 2.7f)
            mDimples.erase(mDimples.begin() + i);
        else
            ++i;
    }
    mDimpleTimer -= mDt;
    if (mDimpleTimer <= 0.0f && (int)mDimples.size() < 12) {
        mDimpleTimer = 0.5f + drng() * 1.1f;
        float ang = drng() * 6.2831853f, rad = 18.0f + drng() * 112.0f; // over the open lake
        mDimples.push_back(glm::vec4(std::cos(ang) * rad, std::sin(ang) * rad, mTime, 0.0f));
    }
    for (size_t i = 0; i < mPulses.size();) {
        mPulses[i].age += mDt;
        if (mPulses[i].age >= mPulses[i].life)
            mPulses.erase(mPulses.begin() + i);
        else
            ++i;
    }

    // Phase 10 feedback timers + a fresh bloom for a just-planted seed.
    mPhotoNote = std::max(0.0f, mPhotoNote - mDt);
    mDawnChorus = std::max(0.0f, mDawnChorus - mDt * 0.5f);
    if (mPlantRequested) {
        reseedSeedPatch();
        mPlantRequested = false;
    }
    // Quiet autosave: persist the vale's accumulated light every so often when dirty.
    if (mPersistEnabled) {
        mSaveTimer += mDt;
        if (mSaveDirty && mSaveTimer >= AUTOSAVE_INTERVAL) {
            saveSession();
            mSaveTimer = 0.0f;
        }
    }

    mFuel = mFuelW / FUEL_MAX;
    updateHeart();
    updateDeer();
    updateBeacons();
    updateConstellations();
    packLights();
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->update();
    }
}

void Window::updateHeart() {
    float p = std::min(1.0f, mCollected / (float)mTarget);
    mHeartP = p;
    glm::vec3 ember(1.0f, 0.25f, 0.05f), gold(1.0f, 0.85f, 0.60f);
    mHeartColor = glm::mix(ember, gold, p);
    mHeartEmissive = glm::mix(0.15f, 4.5f, p);
    if (mLongDawn != Dawn::None) // the Heart breathes ablaze through The Long Dawn
        mHeartEmissive = 5.0f + 0.8f * std::sin(6.2831f * 0.5f * mTime) + mBloom;
    if (mHeart)
        mHeart->setEmissive(glm::vec4(mHeartColor, mHeartEmissive));
}

void Window::addDeer(Player &deer, float x, float z, int herd, float yawOffset) {
    addNPC(deer);
    DeerAgent d;
    d.p = &deer;
    d.x = x;
    d.z = z;
    d.tx = x;
    d.tz = z;
    d.herd = herd;
    d.yawOffset = yawOffset;
    d.pause = 1.0f + 3.0f * ((x * 13.0f + z * 7.0f) - std::floor(x * 13.0f + z * 7.0f));
    if (herd >= (int)mHerdAnchors.size())
        mHerdAnchors.resize(herd + 1, glm::vec2(x, z));
    mDeer.push_back(d);
    deer.setTransform(x, ilo::terrainHeightFast(x, z), z, 0.0f);
}

void Window::updateDeer() {
    if (mState != GameState::Playing && mState != GameState::Intro)
        return;

    // Each herd's anchor wanders slowly across the meadow; the deer graze around it,
    // so the herd reads as a group drifting together rather than scattering.
    for (size_t h = 0; h < mHerdAnchors.size(); h++) {
        glm::vec2 &a = mHerdAnchors[h];
        float t = mTime * 0.05f + (float)h * 2.3f;
        glm::vec2 dir(std::cos(t * 0.7f + std::sin(t)), std::sin(t * 0.9f + std::cos(t * 1.3f)));
        a += dir * (DEER_SPEED * 0.35f * mDt);
        float r = std::sqrt(a.x * a.x + a.y * a.y);
        // Keep herds on the solid meadow ring — never drifting into the central Mere
        // or out past the tree line.
        if (r > 1e-3f) {
            float cr = std::max(195.0f, std::min(340.0f, r));
            a *= cr / r;
        }
    }

    glm::vec2 cam(mCamera.mPosition.x, mCamera.mPosition.z);
    bool rushing = mSprinting || mPlayerSpeed > RUSH_SPEED;

    for (DeerAgent &d : mDeer) {
        float pd = std::sqrt((d.x - cam.x) * (d.x - cam.x) + (d.z - cam.y) * (d.z - cam.y));
        if (d.fleeTimer > 0.0f)
            d.fleeTimer -= mDt;
        // Trust is no-fail and forgiving: warms when you linger close and calm, drops
        // only partially when you rush a deer, and fades slowly when you wander off.
        if (rushing && pd < FLEE_RADIUS && d.fleeTimer <= 0.0f) {
            d.fleeTimer = FLEE_TIME;
            d.trust = std::max(0.0f, d.trust - FLEE_TRUST_DROP);
        } else if (d.fleeTimer <= 0.0f) {
            if (pd < TRUST_RADIUS && mPlayerSpeed < CALM_SPEED)
                d.trust = std::min(1.0f, d.trust + TRUST_GAIN * (1.0f - pd / TRUST_RADIUS) *
                                                       (1.0f - mPlayerSpeed / CALM_SPEED) * mDt);
            if (pd > NOTICE_RADIUS)
                d.trust = std::max(0.0f, d.trust - TRUST_DECAY * mDt);
        }

        // Mood: flee (rushed) -> follow (befriended) -> curious -> wary herd grazing.
        bool faceCam = false;
        float spd = DEER_SPEED;
        bool wander = false;
        if (d.fleeTimer > 0.0f) {
            glm::vec2 away(d.x - cam.x, d.z - cam.y);
            float l = glm::length(away);
            away = l > 1e-4f ? away / l : glm::vec2(0.0f, 1.0f);
            d.tx = d.x + away.x * 8.0f;
            d.tz = d.z + away.y * 8.0f;
            spd = FLEE_SPEED;
        } else if (d.trust >= FOLLOW_THRESH) {
            glm::vec2 toD(d.x - cam.x, d.z - cam.y);
            float l = glm::length(toD);
            glm::vec2 dir = l > 1e-4f ? toD / l : glm::vec2(0.0f, 1.0f);
            d.tx = cam.x + dir.x * FOLLOW_GAP; // settle at your heel, halt facing you
            d.tz = cam.y + dir.y * FOLLOW_GAP;
            spd = FOLLOW_SPEED;
            faceCam = true;
        } else if (d.trust >= CURIOUS_THRESH) {
            if (pd > CURIOUS_GAP) {
                glm::vec2 toC = glm::normalize(glm::vec2(cam.x - d.x, cam.y - d.z));
                d.tx = d.x + toC.x * 1.5f; // edge shyly closer
                d.tz = d.z + toC.y * 1.5f;
            } else {
                d.tx = d.x; // hold and watch
                d.tz = d.z;
            }
            faceCam = true;
        } else {
            wander = true;
        }

        if (wander) {
            float dx = d.tx - d.x, dz = d.tz - d.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < 0.3f) {
                d.pause -= mDt;
                if (d.pause <= 0.0f) {
                    // Graze to a new spot near the herd anchor (cohesion) + personal jitter.
                    float a = std::sin(mTime * 1.3f + d.x * 2.1f + d.z) * 43758.5453f;
                    a = a - std::floor(a);
                    float b = std::sin(mTime * 0.7f + d.z * 1.7f + d.x) * 12543.1234f;
                    b = b - std::floor(b);
                    glm::vec2 anchor = (d.herd < (int)mHerdAnchors.size()) ? mHerdAnchors[d.herd] : glm::vec2(d.x, d.z);
                    float ang = a * 6.2831853f;
                    float rad = 3.0f + 11.0f * b;
                    d.tx = std::max(-520.0f, std::min(520.0f, anchor.x + std::cos(ang) * rad));
                    d.tz = std::max(-520.0f, std::min(520.0f, anchor.y + std::sin(ang) * rad));
                    d.pause = 2.0f + 4.0f * a;
                }
            } else {
                float step = DEER_SPEED * mDt;
                d.x += dx / dist * step;
                d.z += dz / dist * step;
                float target = std::atan2(dx, dz) + d.yawOffset;
                float diff = std::fmod(target - d.yaw + 9.42477796f, 6.2831853f) - 3.14159265f;
                d.yaw += diff * std::min(1.0f, 6.0f * mDt);
            }
        } else {
            // Curious / following / fleeing: walk toward the chosen target, but never
            // step onto the mirror-Mere (revert into-water steps).
            if (ilo::terrainHeightFast(d.tx, d.tz) < DEER_WATER_MIN) {
                d.tx = d.x;
                d.tz = d.z;
            }
            float dx = d.tx - d.x, dz = d.tz - d.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > 0.05f) {
                float step = std::min(spd * mDt, dist); // don't overshoot the heel gap
                d.x += dx / dist * step;
                d.z += dz / dist * step;
            }
            float target = faceCam ? std::atan2(cam.x - d.x, cam.y - d.z) + d.yawOffset
                                   : std::atan2(dx, dz) + d.yawOffset;
            float diff = std::fmod(target - d.yaw + 9.42477796f, 6.2831853f) - 3.14159265f;
            d.yaw += diff * std::min(1.0f, 6.0f * mDt);
        }
        if (d.p)
            d.p->setTransform(d.x, ilo::terrainHeightFast(d.x, d.z), d.z, d.yaw);
    }
}

void Window::updateGlide() {
    // Headless demo hooks (checkEvents is skipped headless, so drive the latches here).
    if (mDemoGlide) {
        mAscendInput = (mTime < 1.6f); // climb briefly, then release into the leaf-fall
        mDescendInput = false;
        if (!mAscendInput && mAirborne && !mDemoGlideSeeded) {
            glm::vec2 f(mCamera.mLookDir.x, mCamera.mLookDir.z);
            if (glm::length(f) > 1e-4f)
                mGlideVel = glm::normalize(f) * GLIDE_SPEED;
            mDemoGlideSeeded = true;
        }
    }
    if (mDemoDeer == 2 && !mDeer.empty()) { // scripted rush so flee is provable headless
        glm::vec3 &cp = mCamera.mPosition;
        glm::vec2 to(mDeer[0].x - cp.x, mDeer[0].z - cp.z);
        float dd = glm::length(to);
        if (dd > 2.5f) {
            to /= dd;
            cp.x += to.x * SPRINT_SPEED * mDt;
            cp.z += to.y * SPRINT_SPEED * mDt;
        }
        mSprinting = true;
    }

    const float floor = 1.8f;
    if (mAscendInput)
        mVertVel = FLY_VERT_SPEED; // powered climb / flap
    else if (mDescendInput)
        mVertVel = -FLY_VERT_SPEED * 1.5f; // active dive
    else if (mEyeOffset > floor + GROUND_EPS)
        mVertVel = std::max(-TERMINAL_FALL, mVertVel - GLIDE_GRAVITY * mDt); // falling leaf
    else
        mVertVel = 0.0f;

    // Wind-rivers: inside a lane an updraft holds/lifts you and the flow sweeps you along.
    bool inRiver = false;
    if (mEyeOffset > floor + GROUND_EPS) {
        glm::vec3 cam = mCamera.mPosition;
        for (const WindRiver &r : mWindRivers) {
            glm::vec3 ab = r.b - r.a;
            float L2 = glm::dot(ab, ab);
            float u = L2 > 1e-4f ? std::max(0.0f, std::min(1.0f, glm::dot(cam - r.a, ab) / L2)) : 0.0f;
            glm::vec3 c = r.a + ab * u;
            if (glm::length(cam - c) < RIVER_RADIUS) {
                inRiver = true;
                mVertVel = std::max(mVertVel, RIVER_LIFT);
                glm::vec3 fd = glm::normalize(ab);
                glm::vec2 flow(fd.x, fd.z);
                mGlideVel += (flow * RIVER_SPEED - mGlideVel) * std::min(1.0f, mDt / RIVER_TAU);
                break;
            }
        }
    }

    mEyeOffset = std::max(floor, std::min(mGlideCap, mEyeOffset + mVertVel * mDt));
    mAirborne = mEyeOffset > floor + GROUND_EPS;

    if (mAirborne) {
        mCamera.mPosition.x += mGlideVel.x * mDt;
        mCamera.mPosition.z += mGlideVel.y * mDt;
        mCamera.mPosition.x = std::max(-WORLD_BOUND, std::min(WORLD_BOUND, mCamera.mPosition.x));
        mCamera.mPosition.z = std::max(-WORLD_BOUND, std::min(WORLD_BOUND, mCamera.mPosition.z));
    } else {
        mVertVel = 0.0f;
        mGlideVel = glm::vec2(0.0f); // rest on landing — no bounce, no drift
    }

    if ((mDemoGlide || mDemoDeer) && (mDbgFrame++ % 15 == 0)) {
        float dist = mDeer.empty() ? 0.0f
                                   : std::sqrt((mDeer[0].x - mCamera.mPosition.x) * (mDeer[0].x - mCamera.mPosition.x) +
                                               (mDeer[0].z - mCamera.mPosition.z) * (mDeer[0].z - mCamera.mPosition.z));
        std::printf("P9 t=%.2f eye=%.2f vy=%.2f glide=%.2f air=%d%s | deer0 trust=%.2f flee=%.2f dist=%.1f\n",
                    mTime, mEyeOffset, mVertVel, glm::length(mGlideVel), (int)mAirborne, inRiver ? " RIVER" : "",
                    mDeer.empty() ? 0.0f : mDeer[0].trust, mDeer.empty() ? 0.0f : mDeer[0].fleeTimer, dist);
    }
    updateWindMotes();
}

void Window::updateWindMotes() {
    if (mWindRivers.empty())
        return;
    const int per = 28;
    std::vector<FieldInstance> inst;
    inst.reserve(mWindRivers.size() * per);
    for (const WindRiver &r : mWindRivers) {
        glm::vec3 ab = r.b - r.a;
        float len = std::max(1.0f, glm::length(ab));
        for (int i = 0; i < per; i++) {
            float t = i / (float)per + mTime * (RIVER_SPEED * 0.12f) / len;
            t = t - std::floor(t); // motes stream along the lane and recycle
            glm::vec3 base = r.a + ab * t;
            float w = i * 1.7f + mTime * 0.8f;
            glm::vec3 off(std::cos(w) * RIVER_RADIUS * 0.5f, std::sin(w * 1.3f) * 2.0f, std::sin(w) * RIVER_RADIUS * 0.5f);
            float fade = std::sin(t * 3.14159f); // dim toward the ends
            FieldInstance fi;
            fi.pos = base + off;
            fi.tintEmissive = glm::vec4(0.6f, 0.85f, 1.0f, 0.5f + 1.4f * fade);
            fi.xform = glm::vec4(0.25f, w, 0.0f, 0.0f);
            inst.push_back(fi);
        }
    }
    mWindMotes.update(inst);
}

void Window::pinSeedStar() {
    if ((int)mWeavePins.size() >= MAX_PINS)
        return;
    glm::vec3 d = glm::normalize(mCamera.mLookDir);
    if (d.y <= 0.06f)
        return; // must be aimed up at the sky, not the ground
    for (const glm::vec3 &p : mWeavePins)
        if (glm::dot(p, d) > 0.9998f)
            return; // too close to an existing pin -> degenerate segment
    mWeavePins.push_back(d);
    mWeaveColor = weavePalette(mConstellations.size());
    mFlash = std::min(0.18f, mFlash + 0.06f); // a soft tick of feedback
}

void Window::clearWeave() { mWeavePins.clear(); }

void Window::completeWeave() {
    if ((int)mWeavePins.size() < 3)
        return;
    int total = 0;
    for (const Constellation &c : mConstellations)
        total += (int)c.stars.size();
    if ((int)mConstellations.size() >= MAX_CONSTELLATIONS || total + (int)mWeavePins.size() > MAX_CSTARS) {
        clearWeave(); // the heavens are full — let the woven sky rest
        return;
    }
    Constellation c;
    c.stars = mWeavePins;
    c.color = weavePalette(mConstellations.size());
    c.bornTime = mTime;
    glm::vec3 cam = mCamera.mPosition;
    for (const glm::vec3 &d : mWeavePins) {
        // The figure "falls" onto the plain along its own bearing: low stars land farther out.
        glm::vec2 h(d.x, d.z);
        float hl = glm::length(h);
        h = hl > 1e-4f ? h / hl : glm::vec2(0.0f, 1.0f);
        float dist = 15.0f + 30.0f * (1.0f - d.y);
        float gx = cam.x + h.x * dist, gz = cam.z + h.y * dist;
        glm::vec3 g(gx, ilo::terrainHeightFast(gx, gz) + 0.4f, gz);
        c.ground.push_back(g);
        if (mPulses.size() < 8) // a warm spark travels down the new chain
            mPulses.push_back({g + glm::vec3(0.0f, 0.6f, 0.0f), c.color, 0.0f, 0.7f});
    }
    mConstellations.push_back(c);
    mWeavePins.clear();
    // The aurora unfurls in the new figure's colour; the world brightens a little.
    mAuroraColor = c.color;
    mAuroraColorTarget = 0.7f;
    mWeaveAuroraBoost = 0.6f;
    mBoonLantern = std::min(1.5f, mBoonLantern + 0.10f);
    mGlideCap = std::min(130.0f, mGlideCap + 10.0f);
    mRadiance = std::min(1.0f, mRadiance + 0.05f);
    mFlash = std::min(0.3f, mFlash + 0.2f);
}

void Window::updateConstellations() {
    mWeaveAuroraBoost *= std::exp(-mDt / 8.0f);
    mAuroraColorMix += (mAuroraColorTarget - mAuroraColorMix) * std::min(1.0f, mDt * 2.0f);
    // Refill the fallen-twin markers: each constellation's chain ignites left-to-right
    // over ~2s then settles to a gentle breathing glow (a permanent record on the plain).
    std::vector<FieldInstance> inst;
    inst.reserve(MAX_CSTARS);
    for (const Constellation &c : mConstellations) {
        float t = mTime - c.bornTime;
        for (size_t i = 0; i < c.ground.size(); i++) {
            const glm::vec3 &pos = c.ground[i];
            float onset = t - 0.18f * (float)i;
            float on = onset <= 0.0f ? 0.0f : (onset >= 1.0f ? 1.0f : onset * onset * (3.0f - 2.0f * onset));
            float breathe = 4.0f + 1.0f * std::sin(6.2831f * 0.4f * mTime + pos.x);
            FieldInstance fi;
            fi.pos = pos;
            fi.tintEmissive = glm::vec4(c.color, breathe * on);
            fi.xform = glm::vec4(0.9f, pos.x * 0.7f, 0.0f, 0.0f);
            inst.push_back(fi);
        }
    }
    mTwinField.update(inst);
}

void Window::seedDemoConstellation() {
    // Headless verification (ILO_DEMO_SKY=1): seed one fully-lit jade constellation in
    // front of the demo camera so a fixed-frame screenshot proves the sky line + colour
    // aurora + fallen-twin ground chain all render.
    mState = GameState::Playing;
    if (!mFreezeDay) { // ensure night so the authored sky is visible
        mDayPhase = 0.0f;
        mFreezeDay = true;
    }
    glm::vec3 look = glm::normalize(mCamera.mLookDir);
    float az0 = std::atan2(look.x, look.z);
    glm::vec3 cam = mCamera.mPosition;
    Constellation c;
    c.color = glm::vec3(0.21f, 1.0f, 0.76f); // jade
    c.bornTime = mTime - 5.0f;               // already lit + steady, deterministic
    for (int k = 0; k < 5; k++) {
        float frac = k / 4.0f - 0.5f;                            // -0.5 .. 0.5
        float az = az0 + frac * 0.7f;                            // fan ±0.35 rad in azimuth
        float el = 0.32f + 0.20f * std::sin((k / 4.0f) * 3.14159f); // an arch
        glm::vec3 d = glm::normalize(glm::vec3(std::sin(az) * std::cos(el), std::sin(el), std::cos(az) * std::cos(el)));
        c.stars.push_back(d);
        glm::vec2 h(d.x, d.z);
        float hl = glm::length(h);
        h = hl > 1e-4f ? h / hl : glm::vec2(0.0f, 1.0f);
        float dist = 15.0f + 30.0f * (1.0f - d.y);
        float gx = cam.x + h.x * dist, gz = cam.z + h.y * dist;
        c.ground.push_back(glm::vec3(gx, ilo::terrainHeightFast(gx, gz) + 0.4f, gz));
    }
    mConstellations.push_back(c);
    mAuroraColor = c.color;
    mAuroraColorMix = mAuroraColorTarget = 0.7f;
    mWeaveAuroraBoost = 0.5f;
    mBoonLantern = std::min(1.5f, mBoonLantern + 0.10f);
    mGlideCap = std::min(130.0f, mGlideCap + 10.0f);
}

void Window::updateLongDawn() {
    if (mLongDawn == Dawn::None) {
        // Earned at full Radiance — playing the whole loop (6 beacons, constellations,
        // fireflies) gets you there. The one-way ratchet guarantees a single break.
        if (mState == GameState::Playing && mRadiance >= LONG_DAWN_THRESHOLD) {
            mLongDawn = Dawn::Flaring;
            mDawnT = 0.0f;
            mDawnChorus = 1.0f;
            mWeaveAuroraBoost += LONG_DAWN_AURORA_SURGE;
            mFlash = std::min(0.45f, mFlash + 0.45f);
            markDirty();
            saveSession();
        }
        mBloom = 0.0f;
    } else if (mLongDawn == Dawn::Flaring) {
        mDawnT += mDt;
        float norm = std::min(1.0f, mDawnT / LONG_DAWN_DURATION);
        mBloom = LONG_DAWN_FLOOR + (LONG_DAWN_PEAK - LONG_DAWN_FLOOR) * std::sin(3.14159265f * norm);
        // A staggered dawn-chorus of light: warm sparks rising across the whole basin.
        if (mDawnChorus > 0.0f && mPulses.size() < 8 && std::fmod(mDawnT, 0.4f) < mDt) {
            float a = std::sin(mDawnT * 12.9898f) * 43758.5453f;
            a -= std::floor(a);
            float b = std::sin(mDawnT * 78.233f) * 12543.0f;
            b -= std::floor(b);
            float ang = a * 6.2831853f, rad = 40.0f + b * 260.0f;
            mPulses.push_back({glm::vec3(std::cos(ang) * rad, 5.0f + b * 9.0f, std::sin(ang) * rad),
                               glm::vec3(1.0f, 0.9f, 0.65f), 0.0f, 1.3f});
        }
        if (mDawnT >= LONG_DAWN_DURATION) {
            mLongDawn = Dawn::Settled;
            mBloom = LONG_DAWN_FLOOR;
            mRadiance = 1.0f;
            mSeedPresent = true; // a new seed glints at the Mere — the endless invitation
            markDirty();
            saveSession();
        }
    } else { // Settled — permanently radiant, forever
        mBloom = LONG_DAWN_FLOOR;
        mRadiance = 1.0f;
    }

    // The breathing seed at the water (present after the dawn / on a remembered vale).
    std::vector<FieldInstance> s;
    if (mSeedPresent) {
        float breathe = 3.0f + 1.0f * std::sin(6.2831f * 0.5f * mTime);
        FieldInstance fi;
        fi.pos = mSeedPos;
        fi.tintEmissive = glm::vec4(0.45f, 1.0f, 0.6f, breathe); // emerald-gold
        fi.xform = glm::vec4(1.4f, mTime * 0.3f, 0.0f, 0.0f);
        s.push_back(fi);
    }
    mSeedField.update(s);
}

void Window::enterRest() {
    mResting = true;
    mGlideVel = glm::vec2(0.0f);
    mVertVel = 0.0f;
    mAirborne = false;
    mEyeOffset = 1.8f; // never rest mid-glide
}
void Window::exitRest() {
    mResting = false;
    mScrubDir = 0;
}

void Window::captureJournal() {
    char name[64];
    std::snprintf(name, sizeof(name), "journal_%d.ppm", mJournalSeq++);
    std::string dir = ".";
#ifndef __EMSCRIPTEN__
    if (const char *d = std::getenv("ILO_JOURNAL_DIR"))
        dir = d;
    else if (const char *h = std::getenv("HOME"))
        dir = std::string(h) + "/lumenmere-journal";
    ::mkdir(dir.c_str(), 0755); // harmless if it already exists
#endif
    mJournalPath = dir + "/" + name;
    mJournalShot = true; // render() grabs the composited, HUD-less frame this pass
    mPhotoNote = PHOTO_NOTE;
    mFlash = std::min(0.25f, mFlash + PHOTO_FLASH);
}

void Window::plantSeed() {
    if (!mSeedPresent)
        return;
    float d = glm::length(glm::vec2(mCamera.mPosition.x - mSeedPos.x, mCamera.mPosition.z - mSeedPos.z));
    if (d <= SEED_PLANT_RADIUS)
        mPlantRequested = true; // bloom happens in update() (GL thread, outside a pass)
}

void Window::reseedSeedPatch() {
    // One bounded, REBUILT field (never unbounded appends) — a fresh wedge of glowing
    // blooms by the water, seeded off how many times you've planted.
    proc::Rng rng(7777u + (unsigned)mSeedsPlanted * 131u);
    glm::vec3 pal[4] = {{1.0f, 0.85f, 0.45f}, {1.0f, 0.55f, 0.75f}, {0.5f, 0.8f, 1.0f}, {0.8f, 0.55f, 1.0f}};
    std::vector<FieldInstance> inst;
    for (int i = 0; i < 180; i++) {
        float ang = rng.range(0.0f, 6.2831853f), rad = rng.range(1.0f, 15.0f);
        float x = mSeedPos.x + std::cos(ang) * rad, z = mSeedPos.z + std::sin(ang) * rad;
        float h = ilo::terrainHeightFast(x, z);
        if (h < 0.6f)
            continue; // keep the new patch out of the Mere
        FieldInstance fi;
        fi.pos = glm::vec3(x, h, z);
        fi.tintEmissive = glm::vec4(pal[(int)(rng.f() * 3.999f) & 3], rng.range(1.6f, 3.0f));
        fi.xform = glm::vec4(rng.range(0.8f, 1.7f), rng.range(0.0f, 6.2831853f), 0.5f, rng.range(0.0f, 6.2831853f));
        inst.push_back(fi);
    }
    mSeedPatchField.update(inst);
    mSeedsPlanted++;
    if (mPulses.size() < 8)
        mPulses.push_back({mSeedPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.5f, 1.0f, 0.6f), 0.0f, 0.9f});
    mFlash = std::min(0.25f, mFlash + 0.12f);
    markDirty();
}

ilo::SaveData Window::gatherSave() const {
    ilo::SaveData s;
    s.radiance = mRadiance;
    s.collected = mCollected;
    s.beaconCount = (int)mBeacons.size();
    for (const Beacon &b : mBeacons)
        s.beaconLit.push_back(b.lit ? 1 : 0);
    for (const Constellation &c : mConstellations) {
        ilo::SavedConstellation sc;
        sc.color = c.color;
        sc.stars = c.stars;
        sc.ground = c.ground;
        s.cons.push_back(sc);
    }
    s.longDawn = (mLongDawn != Dawn::None);
    s.seedsPlanted = mSeedsPlanted;
    s.seedPresent = mSeedPresent;
    return s;
}

void Window::applySave(const ilo::SaveData &s) {
    mRadiance = std::max(0.0f, std::min(1.0f, s.radiance));
    mCollected = s.collected;
    for (size_t i = 0; i < mBeacons.size() && i < s.beaconLit.size(); i++) {
        if (s.beaconLit[i]) {
            mBeacons[i].lit = true;
            mBeacons[i].igniteTime = mTime - 1000.0f; // long past -> no ignite ramp
            if (mWakeEvents.size() < 8) // a FROZEN-radius wake: region reads bloomed, no sweep
                mWakeEvents.push_back(glm::vec4(mBeacons[i].pos.x, mBeacons[i].pos.z, mTime - 1000.0f, WAKE_FROZEN_RADIUS));
        }
    }
    mGrovesAwake = 0;
    for (const Beacon &b : mBeacons)
        if (b.lit)
            mGrovesAwake++;
    mConstellations.clear();
    for (const ilo::SavedConstellation &sc : s.cons) {
        Constellation c;
        c.color = sc.color;
        c.stars = sc.stars;
        c.ground = sc.ground;
        c.bornTime = mTime - 100.0f; // already settled, no chain re-ignite
        mConstellations.push_back(c);
    }
    int n = (int)mConstellations.size();
    mBoonLantern = std::min(1.5f, 1.0f + 0.10f * n);
    mGlideCap = std::min(130.0f, 90.0f + 10.0f * n);
    if (n > 0) {
        mAuroraColor = mConstellations.back().color;
        mAuroraColorMix = mAuroraColorTarget = 0.7f;
    }
    mWeaveAuroraBoost = 0.0f;
    mSeedsPlanted = s.seedsPlanted;
    mSeedPresent = s.seedPresent;
    if (s.longDawn) { // the vale you left was already in eternal dawn
        mLongDawn = Dawn::Settled;
        mBloom = LONG_DAWN_FLOOR;
        mRadiance = 1.0f;
        mDayPhase = DAWN_PHASE;
        mSeedPresent = true;
    }
    updateBeacons();        // rebuild the beacon field to its lit/asleep state
    updateConstellations(); // refill the fallen-twin markers from the loaded figures
}

void Window::saveSession() {
    if (!mPersistEnabled)
        return;
    std::string blob = ilo::serialize(gatherSave());
    mSaveDirty = false;
#ifdef __EMSCRIPTEN__
    ilo_save_blob(blob.c_str());
#else
    if (!mSavePath.empty()) {
        FILE *f = std::fopen(mSavePath.c_str(), "wb");
        if (f) {
            std::fwrite(blob.data(), 1, blob.size(), f);
            std::fclose(f);
        }
    }
#endif
}

void Window::loadSession() {
    if (!mPersistEnabled)
        return;
    std::string blob;
#ifdef __EMSCRIPTEN__
    std::vector<char> buf(ilo::SAVE_MAX_BYTES, 0);
    ilo_load_blob(buf.data(), (int)buf.size());
    blob = buf.data();
#else
    if (!mSavePath.empty()) {
        FILE *f = std::fopen(mSavePath.c_str(), "rb");
        if (f) {
            std::vector<char> buf(ilo::SAVE_MAX_BYTES, 0);
            size_t got = std::fread(buf.data(), 1, buf.size() - 1, f);
            buf[got] = 0;
            std::fclose(f);
            blob = buf.data();
        }
    }
#endif
    ilo::SaveData s;
    if (ilo::deserialize(blob, s))
        applySave(s); // otherwise: a fresh, unremembered vale
}

void Window::resetGame() {
    mFuelW = FUEL_START;
    mCollected = 0;
    mRadiance = 0.0f;
    mGrovesAwake = 0;
    mWakeEvents.clear();
    for (Beacon &b : mBeacons)
        b.lit = false;
    mState = GameState::Intro;
    mIntroTimer = 1.5f;
    mCombo = 0;
    mComboTimer = 0.0f;
    mDawn = 0.0f;
    mFlash = 0.0f;
    mFlareTimer = 0.0f;
    mFlareCooldown = 0.0f;
    mPulses.clear();
    // Phase 8: forget the authored sky and its boons on a fresh start.
    mWeavePins.clear();
    mConstellations.clear();
    mAuroraColorMix = mAuroraColorTarget = mWeaveAuroraBoost = 0.0f;
    mBoonLantern = 1.0f;
    mGlideCap = 90.0f;
    // Phase 9: settle traversal + forget the herd's trust on a fresh start.
    mVertVel = 0.0f;
    mGlideVel = glm::vec2(0.0f);
    mAirborne = false;
    mPlayerSpeed = 0.0f;
    mPrevCamInit = false;
    for (DeerAgent &d : mDeer) {
        d.trust = 0.0f;
        d.fleeTimer = 0.0f;
    }
    // Phase 10: a new vale truly forgets — clear the dawn, rest, seed; write a fresh save.
    mLongDawn = Dawn::None;
    mDawnT = mBloom = mDawnChorus = 0.0f;
    mResting = false;
    mRestDrop = 0.0f;
    mScrubDir = 0;
    mSeedPresent = false;
    mSeedsPlanted = 0;
    mPlantRequested = false;
    mSeedField.update({});
    mSeedPatchField.update({});
    mCamera.mPosition = glm::vec3(0, 2, 18);
    mCamera.setYawPitch(0, 0);
    mFireflies.resetAll(glm::vec3(0, 2, 18));
    saveSession();
}

void Window::renderShadowPass() {
    // Sun-elevation / night fade — skip the whole pass at or below the horizon, where
    // uSunlight is already ~0 so there is nothing to shadow.
    mShadowStrength = mNoShadow ? 0.0f : glm::smoothstep(0.04f, 0.16f, mSky.sunDir.y);
    if (mShadowStrength <= 0.0f)
        return;

    // A camera-centred orthographic frustum in ORIGIN-RELATIVE space, texel-snapped so
    // the shadow is world-locked and never shimmers as the camera walks.
    glm::vec3 center = mCamera.mPosition - mRenderOrigin;
    glm::vec3 sun = glm::normalize(mSky.sunDir); // toward the sun
    const float R = mShadowRadius, D = 250.0f;
    glm::vec3 up(0.0f, 1.0f, 0.0f); // Sky.sunDir keeps a -0.35 z, never parallel to +Y
    glm::mat4 lightView = glm::lookAt(center + sun * D, center, up);
    glm::mat4 lightProj = glm::ortho(-R, R, -R, R, 0.0f, 2.0f * D);
    float texelWorld = 2.0f * R / (float)mShadowRes;
    glm::vec4 cLS = lightView * glm::vec4(center, 1.0f);
    cLS.x = std::floor(cLS.x / texelWorld) * texelWorld;
    cLS.y = std::floor(cLS.y / texelWorld) * texelWorld;
    glm::vec3 cWS = glm::vec3(glm::inverse(lightView) * glm::vec4(cLS.x, cLS.y, cLS.z, 1.0f));
    lightView = glm::lookAt(cWS + sun * D, cWS, up);
    mLightVP = lightProj * lightView;

    shadowFBO.bind();
    glViewport(0, 0, mShadowRes, mShadowRes);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f); // slope-scaled depth bias against acne
    glClear(GL_DEPTH_BUFFER_BIT);

    shadowProg.useProgram();
    GLuint pid = shadowProg.getProgramID();
    glUniformMatrix4fv(glGetUniformLocation(pid, "uLightVP"), 1, GL_FALSE, glm::value_ptr(mLightVP));
    glUniform3f(glGetUniformLocation(pid, "uOriginOffset"), mRenderOrigin.x, mRenderOrigin.y, mRenderOrigin.z);
    glUniform2f(glGetUniformLocation(pid, "uWind"), 0.45f, 0.30f); // identical to the geometry pass
    glUniform1f(glGetUniformLocation(pid, "time"), mTime);
    glVertexAttrib4f(6, 1.0f, 0.0f, 0.0f, 0.0f); // default per-instance xform (Player/Mushroom)

    // Closed OBJ casters with back-face cull, then double-sided procedural casters —
    // same order as renderGeometryPass so model/grassWave uniform inheritance matches.
    glEnable(GL_CULL_FACE);
    for (unsigned int i = 0; i < mGameObjects.size(); i++)
        mGameObjects[i]->render(pid);
    glDisable(GL_CULL_FACE);
    mTerrain.render(pid);
    if (mProps)
        mProps->render(pid);
    for (auto &f : mFields)
        f.render(pid);
    mMushrooms.render(pid, mTime);
    // Skipped casters (still RECEIVE shadows): grass/motes/seed/fireflies/birds/
    // butterflies/beacons — animated/glowy/tiny; casting them would shimmer or need
    // bespoke sway replication.

    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderGeometryPass() {
    glViewport(0, 0, mWidth, mHeight);
    gBuffer.bind();
    gBuffer.setDrawBuffers();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    geometryProg.useProgram();
    GLuint pid = geometryProg.getProgramID();
    glm::mat4 persp = projection();
    glUniformMatrix4fv(glGetUniformLocation(pid, "persp"), 1, GL_FALSE, glm::value_ptr(persp));
    glUniformMatrix4fv(glGetUniformLocation(pid, "view"), 1, GL_FALSE, glm::value_ptr(mCamera.mView));
    glUniform3f(glGetUniformLocation(pid, "uOriginOffset"), mRenderOrigin.x, mRenderOrigin.y, mRenderOrigin.z);
    glUniform2f(glGetUniformLocation(pid, "uWind"), 0.45f, 0.30f);
    glUniform3f(glGetUniformLocation(pid, "uPlayerPos"), mCamera.mPosition.x, mCamera.mPosition.y, mCamera.mPosition.z);
    glUniform1i(glGetUniformLocation(pid, "uWakeCount"), (int)mWakeEvents.size());
    if (!mWakeEvents.empty())
        glUniform4fv(glGetUniformLocation(pid, "uWake"), (GLsizei)mWakeEvents.size(), (const float *)mWakeEvents.data());
    glUniform1f(glGetUniformLocation(pid, "time"), mTime);
    glUniform1f(glGetUniformLocation(pid, "uBloom"), mBloom); // The Long Dawn unison flare
    // Default per-instance transform for non-field geometry (scale 1, no yaw/wind).
    glVertexAttrib4f(6, 1.0f, 0.0f, 0.0f, 0.0f);

    // Closed OBJ meshes (forest, deer, Heart) render with back-face culling.
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->render(pid);
    }
    // Procedurally generated meshes are drawn double-sided (their winding varies).
    glDisable(GL_CULL_FACE);
    mTerrain.render(pid);
    if (mProps)
        mProps->render(pid);
    for (auto &f : mFields)
        f.render(pid);
    mGrass.render(pid);
    mBeaconField.render(pid);
    mTwinField.render(pid);
    mWindMotes.render(pid);
    mSeedField.render(pid);
    mSeedPatchField.render(pid);
    mBirds.render(pid);
    mButterflies.render(pid);
    mMushrooms.render(pid, mTime);
    mFireflies.render(pid, mTime);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderSSAO() {
    // Occlusion pass: gather a hemisphere from the G-buffer into the half-res buffer.
    ssaoFBO.bind();
    glViewport(0, 0, ssaoFBO.w, ssaoFBO.h);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    ssaoProg.useProgram();
    GLuint pid = ssaoProg.getProgramID();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(1));
    glm::vec3 eyeRel = mCamera.mPosition - mRenderOrigin;
    glUniform3f(glGetUniformLocation(pid, "eyePos"), eyeRel.x, eyeRel.y, eyeRel.z);
    // origin-relative world -> clip: proj * view * translate(origin).
    glm::mat4 vpRel = projection() * mCamera.mView * glm::translate(glm::mat4(1.0f), mRenderOrigin);
    glUniformMatrix4fv(glGetUniformLocation(pid, "uViewProjRel"), 1, GL_FALSE, glm::value_ptr(vpRel));
    tri.draw();

    // Blur pass: average the 4x4 noise dither out of the raw AO.
    ssaoBlurFBO.bind();
    glViewport(0, 0, ssaoBlurFBO.w, ssaoBlurFBO.h);
    ssaoBlurProg.useProgram();
    glUniform2f(glGetUniformLocation(ssaoBlurProg.getProgramID(), "uTexel"), 1.0f / ssaoFBO.w, 1.0f / ssaoFBO.h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoFBO.color(0));
    tri.draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderSky() {
    glViewport(0, 0, skyFBO.w, skyFBO.h);
    skyFBO.bind();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    skyProg.useProgram();
    GLuint pid = skyProg.getProgramID();
    glm::mat4 invVP = glm::inverse(projection() * mCamera.mView);
    glUniformMatrix4fv(glGetUniformLocation(pid, "invViewProj"), 1, GL_FALSE, glm::value_ptr(invVP));
    glUniform3f(glGetUniformLocation(pid, "eyePos"), mCamera.mPosition.x, mCamera.mPosition.y, mCamera.mPosition.z);
    glUniform1f(glGetUniformLocation(pid, "uTime"), mTime);
    auto v3 = [&](const char *n, glm::vec3 v) { glUniform3f(glGetUniformLocation(pid, n), v.x, v.y, v.z); };
    auto f1 = [&](const char *n, float v) { glUniform1f(glGetUniformLocation(pid, n), v); };
    v3("uSkyTop", mSky.skyTop);
    v3("uSkyHorizon", mSky.skyHorizon);
    v3("uHorizonGlow", mSky.horizonGlow);
    v3("uSunDir", mSky.sunDir);
    v3("uMoonDir", mSky.moonDir);
    v3("uSunDiscColor", mSky.sunDiscColor);
    v3("uMoonColor", mSky.moonColor);
    v3("uSunlight", mSky.sunlight);
    f1("uSunDiscSize", mSky.sunDiscSize);
    f1("uMoonSize", mSky.moonSize);
    f1("uStarFade", mSky.starFade);
    f1("uAuroraStrength", mSky.auroraStrength + mWeaveAuroraBoost); // weave surge rides along
    f1("uGalaxyStrength", mSky.galaxyStrength);
    f1("uCloudCoverage", mSky.cloudCoverage);
    glUniform2f(glGetUniformLocation(pid, "uCloudWind"), 0.006f, 0.004f);

    // Constellation weaving: in-progress pins + the live preview thread, the persisted
    // figures (each in its own hue), and the aurora's woven colour.
    glUniform1i(glGetUniformLocation(pid, "uPinCount"), (int)mWeavePins.size());
    if (!mWeavePins.empty())
        glUniform3fv(glGetUniformLocation(pid, "uPins"), (GLsizei)mWeavePins.size(), (const float *)mWeavePins.data());
    v3("uWeaveColor", mWeaveColor);
    v3("uWeaveCursor", glm::normalize(mCamera.mLookDir));
    {
        glm::vec4 cstars[MAX_CSTARS];
        glm::vec3 ccols[MAX_CSTARS];
        int n = 0;
        for (const Constellation &c : mConstellations) {
            for (size_t j = 0; j < c.stars.size() && n < MAX_CSTARS; j++, n++) {
                cstars[n] = glm::vec4(c.stars[j], j == 0 ? 0.0f : 1.0f); // .w links to previous
                ccols[n] = c.color;
            }
        }
        glUniform1i(glGetUniformLocation(pid, "uCStarCount"), n);
        if (n > 0) {
            glUniform4fv(glGetUniformLocation(pid, "uCStars"), n, (const float *)cstars);
            glUniform3fv(glGetUniformLocation(pid, "uCStarColor"), n, (const float *)ccols);
        }
    }
    v3("uAuroraColor", mAuroraColor);
    f1("uAuroraColorMix", mAuroraColorMix);
    tri.draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderLightingPass() {
    glViewport(0, 0, mWidth, mHeight);
    hdrFBO.bind();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    lightingProg.useProgram();
    GLuint pid = lightingProg.getProgramID();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(1));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(2));
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(3));
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, skyFBO.color(0));
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurFBO.color(0));
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, shadowFBO.depth());

    // eyePos in origin-relative space (matches the G-buffer positions and lights).
    glm::vec3 eyeRel = mCamera.mPosition - mRenderOrigin;
    glUniform3f(glGetUniformLocation(pid, "eyePos"), eyeRel.x, eyeRel.y, eyeRel.z);
    // Hemisphere ambient: a cool sky tint from above, a warm dim bounce from below.
    glm::vec3 ambSky = mSky.ambient * glm::vec3(0.92f, 0.98f, 1.14f);
    glm::vec3 ambGround = mSky.ambient * glm::vec3(1.12f, 0.96f, 0.72f) * 0.6f;
    glUniform3f(glGetUniformLocation(pid, "uAmbient"), ambSky.x, ambSky.y, ambSky.z);
    glUniform3f(glGetUniformLocation(pid, "uAmbientGround"), ambGround.x, ambGround.y, ambGround.z);
    glUniform3f(glGetUniformLocation(pid, "uFogColor"), mSky.fogColor.x, mSky.fogColor.y, mSky.fogColor.z);
    glUniform1f(glGetUniformLocation(pid, "uFogDensity"), mSky.fogDensity);
    glUniform1f(glGetUniformLocation(pid, "uFogHeightFalloff"), mFogHeightFalloff);
    glUniform1f(glGetUniformLocation(pid, "uFogBaseY"), mFogBaseY);
    glUniform3f(glGetUniformLocation(pid, "uSunDir"), mSky.sunDir.x, mSky.sunDir.y, mSky.sunDir.z);
    glUniform3f(glGetUniformLocation(pid, "uSunlight"), mSky.sunlight.x, mSky.sunlight.y, mSky.sunlight.z);
    glm::vec3 rim = (mSky.skyHorizon * 0.9f + mSky.sunlight * 0.25f + mSky.ambient * 2.0f) * 1.5f;
    glUniform3f(glGetUniformLocation(pid, "uRimColor"), rim.x, rim.y, rim.z);

    // Sun shadow map.
    glUniformMatrix4fv(glGetUniformLocation(pid, "uLightVP"), 1, GL_FALSE, glm::value_ptr(mLightVP));
    glUniform1f(glGetUniformLocation(pid, "uShadowTexel"), 1.0f / (float)mShadowRes);
    glUniform1f(glGetUniformLocation(pid, "uShadowTexelWorld"), 2.0f * mShadowRadius / (float)mShadowRes);
    glUniform1f(glGetUniformLocation(pid, "uShadowBias"), 0.0008f);
    glUniform1f(glGetUniformLocation(pid, "uShadowStrength"), mNoShadow ? 0.0f : mShadowStrength);
    glUniform1i(glGetUniformLocation(pid, "uShadowDebug"), mShadowDebug);

    lightUBO.bindBase(0);
    tri.draw();
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderWater() {
    // Blit the scene depth into the HDR buffer so the water plane is occluded by land.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrFBO.fbo);
    glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    hdrFBO.bind();
    glViewport(0, 0, mWidth, mHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    waterProg.useProgram();
    GLuint pid = waterProg.getProgramID();
    glm::mat4 persp = projection();
    glUniformMatrix4fv(glGetUniformLocation(pid, "persp"), 1, GL_FALSE, glm::value_ptr(persp));
    glUniformMatrix4fv(glGetUniformLocation(pid, "view"), 1, GL_FALSE, glm::value_ptr(mCamera.mView));
    glUniform3f(glGetUniformLocation(pid, "eyePos"), mCamera.mPosition.x, mCamera.mPosition.y, mCamera.mPosition.z);
    glUniform2f(glGetUniformLocation(pid, "uScreen"), (float)mWidth, (float)mHeight);
    glUniform1f(glGetUniformLocation(pid, "uTime"), mTime);
    auto v3 = [&](const char *n, glm::vec3 v) { glUniform3f(glGetUniformLocation(pid, n), v.x, v.y, v.z); };
    v3("uSkyTop", mSky.skyTop);
    v3("uSkyHorizon", mSky.skyHorizon);
    v3("uHorizonGlow", mSky.horizonGlow);
    v3("uSunDir", mSky.sunDir);
    v3("uSunColor", mSky.sunDiscColor);
    v3("uMoonDir", mSky.moonDir);
    v3("uMoonColor", mSky.moonColor);
    v3("uWaterColor", glm::vec3(0.015f, 0.055f, 0.075f));
    glUniform1f(glGetUniformLocation(pid, "uStarFade"), mSky.starFade);
    glUniform3f(glGetUniformLocation(pid, "uAuroraColor"), mAuroraColor.x, mAuroraColor.y, mAuroraColor.z);
    glUniform1f(glGetUniformLocation(pid, "uAuroraColorMix"), mAuroraColorMix);
    glUniform1i(glGetUniformLocation(pid, "uDimpleCount"), (int)mDimples.size());
    if (!mDimples.empty())
        glUniform4fv(glGetUniformLocation(pid, "uDimple"), (GLsizei)mDimples.size(), (const float *)mDimples.data());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.color(0));
    glBindVertexArray(mWaterVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderGodrays() {
    godrayFBO.bind();
    glViewport(0, 0, godrayFBO.w, godrayFBO.h);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Project the sun onto the screen and decide how strongly shafts show.
    glm::vec4 clip = projection() * mCamera.mView * glm::vec4(mCamera.mPosition + mSky.sunDir * 1000.0f, 1.0f);
    float strength = 0.0f;
    glm::vec2 sunUV(0.5f);
    if (clip.w > 0.0f) {
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        sunUV = glm::vec2(ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f);
        float elev = mSky.sunDir.y;
        float vis = glm::smoothstep(-0.05f, 0.18f, elev) * (1.0f - glm::smoothstep(0.55f, 0.88f, elev));
        float onx = 1.0f - glm::smoothstep(0.5f, 1.3f, std::abs(ndc.x));
        float ony = 1.0f - glm::smoothstep(0.5f, 1.3f, std::abs(ndc.y));
        strength = vis * onx * ony * 0.7f;
    }

    if (strength > 0.001f) {
        godrayProg.useProgram();
        GLuint pid = godrayProg.getProgramID();
        glUniform2f(glGetUniformLocation(pid, "uSunScreen"), sunUV.x, sunUV.y);
        glUniform1f(glGetUniformLocation(pid, "uStrength"), strength);
        glUniform3f(glGetUniformLocation(pid, "uSunColor"), mSky.sunDiscColor.x, mSky.sunDiscColor.y, mSky.sunDiscColor.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrFBO.color(0));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gBuffer.color(1));
        tri.draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderParticles() {
    if (!mParticleVao)
        return;
    hdrFBO.bind(); // hdr depth holds the scene (blitted in the water pass) -> motes occlude
    glViewport(0, 0, mWidth, mHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // additive glow
#ifndef __EMSCRIPTEN__
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif
    particleProg.useProgram();
    GLuint pid = particleProg.getProgramID();
    glm::mat4 persp = projection();
    glUniformMatrix4fv(glGetUniformLocation(pid, "persp"), 1, GL_FALSE, glm::value_ptr(persp));
    glUniformMatrix4fv(glGetUniformLocation(pid, "view"), 1, GL_FALSE, glm::value_ptr(mCamera.mView));
    glUniform3f(glGetUniformLocation(pid, "uEye"), mCamera.mPosition.x, mCamera.mPosition.y, mCamera.mPosition.z);
    glUniform1f(glGetUniformLocation(pid, "uTime"), mTime);
    glUniform1f(glGetUniformLocation(pid, "uBoxR"), 40.0f);
    glUniform3f(glGetUniformLocation(pid, "uDrift"), 0.6f, 0.35f, 0.4f);
    glUniform1f(glGetUniformLocation(pid, "uSizePx"), 220.0f);
    float night = mSky.nightAmount;
    glm::vec3 col = glm::mix(glm::vec3(0.9f, 0.85f, 0.6f) * 0.30f, glm::vec3(1.0f, 0.5f, 0.2f) * 0.45f, night);
    glUniform3f(glGetUniformLocation(pid, "uColor"), col.x, col.y, col.z);
    glBindVertexArray(mParticleVao);
    glDrawArrays(GL_POINTS, 0, mParticleCount);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderBloom() {
    if (!mBloomReady)
        return;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, bloomA.w, bloomA.h);

    // Bright-pass: extract the bright parts of the HDR scene into the half-res buffer.
    brightProg.useProgram();
    GLuint bp = brightProg.getProgramID();
    glUniform1f(glGetUniformLocation(bp, "uThreshold"), 1.0f);
    glUniform1f(glGetUniformLocation(bp, "uSoftKnee"), 0.5f);
    bloomA.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrFBO.color(0));
    tri.draw();

    // Separable Gaussian, ping-ponging between the two half-res buffers.
    blurProg.useProgram();
    GLuint blp = blurProg.getProgramID();
    int draws = mLowSpec ? 4 : 10;
    bool horizontal = true;
    ilo::Framebuffer *src = &bloomA;
    ilo::Framebuffer *dst = &bloomB;
    for (int i = 0; i < draws; ++i) {
        dst->bind();
        glUniform1i(glGetUniformLocation(blp, "horizontal"), horizontal ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src->color(0));
        tri.draw();
        std::swap(src, dst);
        horizontal = !horizontal;
    }
    mBloomTex = src->color(0); // last buffer written
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::renderComposite() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mWidth, mHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    compositeProg.useProgram();
    GLuint pid = compositeProg.getProgramID();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrFBO.color(0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (mBloomReady && mBloomTex) ? mBloomTex : mBlackTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, godrayFBO.color(0));
    glUniform1f(glGetUniformLocation(pid, "uTime"), mTime);
    glUniform1f(glGetUniformLocation(pid, "uExposure"), mExposure);
    glUniform1f(glGetUniformLocation(pid, "uBloomIntensity"), (mBloomReady && mBloomTex) ? mBloomIntensity : 0.0f);
    glUniform1f(glGetUniformLocation(pid, "uVignetteMax"), mVignetteMax);
    glUniform1f(glGetUniformLocation(pid, "uFuel"), mFuel);
    glUniform1f(glGetUniformLocation(pid, "uFlash"), mFlash);
    tri.draw();
}

void Window::renderHud() {
    mHud.begin(mWidth, mHeight);
    float f = mFuel;
    glm::vec4 warm(1.0f, 0.95f, 0.8f, 1.0f);
    char buf[80];

    // Rest / Observe: clear the screen of gameplay UI and just let the vale fill it.
    // Only a fading keepsake note and a quiet scrub cue remain.
    if (mResting) {
        if (mPhotoNote > 0.0f)
            mHud.textCentered(0.5f, 0.5f, 0.026f, "a moment kept",
                              glm::vec4(1.0f, 0.97f, 0.85f, std::min(0.7f, mPhotoNote)));
        mHud.textCentered(0.5f, 0.93f, 0.020f, "RESTING   < >  SCRUB THE SKY    [T] RISE    [G] PHOTO",
                          glm::vec4(0.8f, 0.85f, 0.95f, 0.45f));
        mHud.end();
        return;
    }

    // Firefly counter + world radiance (top-left).
    std::snprintf(buf, sizeof(buf), "FIREFLIES  %d / %d", std::min(mCollected, mTarget), mTarget);
    mHud.text(0.04f, 0.05f, 0.040f, buf, warm);
    std::snprintf(buf, sizeof(buf), "RADIANCE  %d%%", (int)(mRadiance * 100.0f + 0.5f));
    mHud.text(0.04f, 0.10f, 0.030f, buf, glm::vec4(0.78f, 0.88f, 1.0f, 0.85f));
    std::snprintf(buf, sizeof(buf), "GROVES  %d / %d", mGrovesAwake, (int)mBeacons.size());
    mHud.text(0.04f, 0.135f, 0.026f, buf, glm::vec4(1.0f, 0.85f, 0.55f, 0.8f));

    // Warmth meter (bottom-left).
    mHud.text(0.04f, 0.860f, 0.026f, "WARMTH", warm);
    mHud.rect(0.04f, 0.90f, 0.30f, 0.035f, glm::vec4(0.05f, 0.05f, 0.07f, 0.6f));
    glm::vec4 fill = f > 0.5f ? glm::vec4(1.0f, 0.75f, 0.4f, 1.0f)
                              : (f > 0.25f ? glm::vec4(1.0f, 0.5f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.2f, 0.1f, 1.0f));
    if (f <= 0.25f)
        fill.a *= 0.6f + 0.4f * std::sin(6.2831f * 2.0f * mTime);
    if (f > 0.001f)
        mHud.rect(0.04f, 0.90f, 0.30f * f, 0.035f, fill);

    // Combo indicator while a chain is active.
    if (mCombo >= 2 && mComboTimer > 0.0f) {
        std::snprintf(buf, sizeof(buf), "COMBO x%d", mCombo);
        float a = std::min(1.0f, mComboTimer / 1.5f);
        mHud.text(0.04f, 0.15f, 0.030f, buf, glm::vec4(1.0f, 0.85f, 0.4f, a));
    }

    // A calm, non-urgent nudge toward the light when the lantern runs low (no alarm).
    if (mState == GameState::Playing && f <= 0.18f)
        mHud.textCentered(0.5f, 0.13f, 0.030f, "WANDER TOWARD THE LIGHT", glm::vec4(0.75f, 0.82f, 1.0f, 0.7f));

    // Stargazing: a faint reticle appears when you look skyward (where you can pin a
    // seed-star), and a calm weave hint while a constellation is in progress (Phase 8).
    if (mState == GameState::Playing && mCamera.mLookDir.y > 0.06f) {
        glm::vec4 dot = mWeavePins.empty() ? glm::vec4(0.70f, 0.80f, 1.0f, 0.30f) : glm::vec4(mWeaveColor, 0.9f);
        mHud.rect(0.4985f, 0.497f, 0.0030f, 0.0055f, dot);
    }
    if (mState == GameState::Playing && !mWeavePins.empty()) {
        std::snprintf(buf, sizeof(buf), "WEAVING  %d / %d", (int)mWeavePins.size(), MAX_PINS);
        mHud.textCentered(0.5f, 0.20f, 0.026f, buf, glm::vec4(mWeaveColor, 0.9f));
        mHud.textCentered(0.5f, 0.235f, 0.020f, "[RMB] PIN   [F] FINISH   [Q] CLEAR",
                          glm::vec4(0.8f, 0.85f, 0.95f, 0.6f));
    }

    // A soft, fading note when a deer befriends you (Phase 9). No meters, no numbers.
    if (mState == GameState::Playing) {
        float best = 0.0f;
        for (const DeerAgent &d : mDeer)
            best = std::max(best, d.fleeTimer > 0.0f ? 0.0f : d.trust);
        if (best >= FOLLOW_THRESH)
            mHud.textCentered(0.5f, 0.16f, 0.024f, "a deer walks with you", glm::vec4(0.85f, 0.92f, 0.8f, 0.55f));
        else if (best >= CURIOUS_THRESH)
            mHud.textCentered(0.5f, 0.16f, 0.024f, "a deer watches you", glm::vec4(0.82f, 0.88f, 0.95f, 0.5f));
    }

    if (mState == GameState::Intro) {
        int n = (int)std::ceil(mIntroTimer);
        n = std::max(1, std::min(3, n));
        std::snprintf(buf, sizeof(buf), "%d", n);
        mHud.textCentered(0.5f, 0.30f, 0.045f, "FIREFLY GROVE", warm);
        mHud.textCentered(0.5f, 0.42f, 0.12f, buf, warm);
    }
    if (mState == GameState::Paused) {
        mHud.rect(0, 0, 1, 1, glm::vec4(0, 0, 0, 0.5f));
        mHud.textCentered(0.5f, 0.42f, 0.07f, "PAUSED", warm);
        mHud.textCentered(0.5f, 0.54f, 0.035f, "[P] RESUME", warm);
    }
    // No win/lose screen — the only ending is The Long Dawn, and it stays in the world.
    // A single, brief, in-world line marks the moment it breaks; then silence.
    if (mLongDawn == Dawn::Flaring && mDawnT < 6.0f) {
        float a = std::min(0.85f, mDawnT * 0.6f) * std::min(1.0f, (6.0f - mDawnT));
        mHud.textCentered(0.5f, 0.30f, 0.05f, "THE LONG DAWN", glm::vec4(1.0f, 0.92f, 0.7f, a));
    }
    // A fading keepsake confirmation, and a quiet whisper toward the Mere's new seed.
    if (mPhotoNote > 0.0f)
        mHud.textCentered(0.5f, 0.50f, 0.026f, "a moment kept", glm::vec4(1.0f, 0.97f, 0.85f, std::min(0.7f, mPhotoNote)));
    if (mLongDawn == Dawn::Settled && mSeedPresent) {
        float d = glm::length(glm::vec2(mCamera.mPosition.x - mSeedPos.x, mCamera.mPosition.z - mSeedPos.z));
        if (d < 26.0f)
            mHud.textCentered(0.5f, 0.78f, 0.022f, d <= SEED_PLANT_RADIUS ? "[E]  plant the seed" : "a new seed waits at the water",
                              glm::vec4(0.6f, 1.0f, 0.7f, 0.55f));
    }

    mHud.end();
}

void Window::render() {
    renderShadowPass();
    renderGeometryPass();
    renderSSAO();
    renderSky();
    renderLightingPass();
    renderWater();
    renderGodrays();
    renderParticles();
    renderBloom();
    renderComposite();
    // Field Journal: grab the fully-composited, HUD-LESS frame as a clean keepsake.
    if (mJournalShot) {
        ilo::savePPM(mJournalPath, mWidth, mHeight);
        mJournalShot = false;
    }
    renderHud();

    if (mPendingShot) { // capture the freshly-rendered back buffer before presenting
        ilo::savePPM(mShotPath, mWidth, mHeight);
        mPendingShot = false;
    }
    SDL_GL_SwapWindow(mSDLwindow);
    float currentTime = SDL_GetTicks();
    frames++;
    if (currentTime - lastTime >= 1000) {
        std::cout << "Frames: " << frames << std::endl;
        frames = 0;
        lastTime = currentTime;
    }
}

void Window::resize() {
    int w = mWidth, h = mHeight;
    SDL_GL_GetDrawableSize(mSDLwindow, &w, &h);
    if (w == 0 || h == 0 || (w == mWidth && h == mHeight))
        return;
    mWidth = w;
    mHeight = h;
    destroyFramebuffers();
    createFramebuffers();
    glViewport(0, 0, mWidth, mHeight);
}

void Window::setCamera(Camera &c) {
    mCamera = c;
}

void Window::setvSync(bool vSyncStatus) {
    vSync = vSyncStatus;
}
void Window::setMouseSensitivity(float _sensitivity) {
    mMouseSensitivity = _sensitivity;
}
void Window::setScrollSensitivity(float) {}
void Window::setSSAA(float) {}
void Window::setFOV(float _fov) {
    if (0 < _fov && _fov < 3.1415f)
        mFOV = _fov;
}

void Window::addNPC(Player &npc) {
    baseObjects++;
    mGameObjects.push_back(&npc);
}
