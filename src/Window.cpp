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
#else
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

    // Static sampler bindings.
    lightingProg.useProgram();
    GLuint lp = lightingProg.getProgramID();
    glUniform1i(glGetUniformLocation(lp, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lp, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lp, "gAlbedo"), 2);
    glUniform1i(glGetUniformLocation(lp, "gMtlProps"), 3);
    glUniform1i(glGetUniformLocation(lp, "uSkyTex"), 4);
    glUniform1i(glGetUniformLocation(lp, "uAO"), 5);
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
            break;
        }
    }
#endif
}

void Window::checkEvents() {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    bool canMove = (mState == GameState::Playing || mState == GameState::Intro);
    bool sprint = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    bool moving = state[SDL_SCANCODE_W] || state[SDL_SCANCODE_S] || state[SDL_SCANCODE_A] || state[SDL_SCANCODE_D];
    mSprinting = canMove && sprint && moving;
    mMoving = canMove && moving;
    float speed = (sprint ? SPRINT_SPEED : WALK_SPEED) * mDt;
    if (canMove) {
        if (state[SDL_SCANCODE_W])
            mCamera.moveForward(speed);
        if (state[SDL_SCANCODE_S])
            mCamera.moveForward(-speed);
        if (state[SDL_SCANCODE_A])
            mCamera.moveRight(-speed);
        if (state[SDL_SCANCODE_D])
            mCamera.moveRight(speed);
        // Rise/sink relative to the ground (ground-follow keeps you on the surface
        // at mEyeOffset; raising it lets you drift up and glide tranquilly).
        if (state[SDL_SCANCODE_SPACE])
            mEyeOffset = std::min(90.0f, mEyeOffset + FLY_VERT_SPEED * mDt);
        if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_C])
            mEyeOffset = std::max(1.8f, mEyeOffset - FLY_VERT_SPEED * mDt);
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
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                resize();
            break;
        case SDL_QUIT:
            closed = true;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
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
        if (mState == GameState::Won)
            intensity = 4.5f + 1.0f * std::sin(6.2831f * 0.5f * mTime);
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
    // Ground-follow: ride the terrain at eye height (raise mEyeOffset to fly up).
    if (!mFreeCam) {
        float g = ilo::terrainHeightFast(mCamera.mPosition.x, mCamera.mPosition.z) + mEyeOffset;
        mCamera.mPosition.y = g;
        mCamera.update();
    }

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

        if (mCollected >= mTarget)
            mState = GameState::Won;
    }

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
    // Advance the day-night cycle; on a win, ease toward sunrise so dawn breaks.
    if (!mFreezeDay) {
        mDayPhase += mDt / mDayLength;
        if (mState == GameState::Won)
            mDayPhase += (0.23f - mDayPhase) * std::min(1.0f, mDt * 0.25f);
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

    mFuel = mFuelW / FUEL_MAX;
    updateHeart();
    updateDeer();
    updateBeacons();
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
    if (mState == GameState::Won) // breathe when fully ablaze
        mHeartEmissive = 5.0f + 0.8f * std::sin(6.2831f * 0.5f * mTime);
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

    for (DeerAgent &d : mDeer) {
        float dx = d.tx - d.x, dz = d.tz - d.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < 0.3f) {
            d.pause -= mDt;
            if (d.pause <= 0.0f) {
                // Graze to a new spot near the herd anchor (cohesion), with a little
                // personal jitter so the deer don't stack on one point.
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
            // Smoothly turn toward the heading (shortest angular path) so the deer
            // bank into their walk instead of snapping around.
            float target = std::atan2(dx, dz) + d.yawOffset;
            float diff = std::fmod(target - d.yaw + 9.42477796f, 6.2831853f) - 3.14159265f;
            d.yaw += diff * std::min(1.0f, 6.0f * mDt);
        }
        if (d.p)
            d.p->setTransform(d.x, ilo::terrainHeightFast(d.x, d.z), d.z, d.yaw);
    }
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
    mCamera.mPosition = glm::vec3(0, 2, 18);
    mCamera.setYawPitch(0, 0);
    mFireflies.resetAll(glm::vec3(0, 2, 18));
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
    f1("uAuroraStrength", mSky.auroraStrength);
    f1("uGalaxyStrength", mSky.galaxyStrength);
    f1("uCloudCoverage", mSky.cloudCoverage);
    glUniform2f(glGetUniformLocation(pid, "uCloudWind"), 0.006f, 0.004f);
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

    // eyePos in origin-relative space (matches the G-buffer positions and lights).
    glm::vec3 eyeRel = mCamera.mPosition - mRenderOrigin;
    glUniform3f(glGetUniformLocation(pid, "eyePos"), eyeRel.x, eyeRel.y, eyeRel.z);
    glUniform3f(glGetUniformLocation(pid, "uAmbient"), mSky.ambient.x, mSky.ambient.y, mSky.ambient.z);
    glUniform3f(glGetUniformLocation(pid, "uFogColor"), mSky.fogColor.x, mSky.fogColor.y, mSky.fogColor.z);
    glUniform1f(glGetUniformLocation(pid, "uFogDensity"), mSky.fogDensity);
    glUniform1f(glGetUniformLocation(pid, "uFogHeightFalloff"), mFogHeightFalloff);
    glUniform1f(glGetUniformLocation(pid, "uFogBaseY"), mFogBaseY);
    glUniform3f(glGetUniformLocation(pid, "uSunDir"), mSky.sunDir.x, mSky.sunDir.y, mSky.sunDir.z);
    glUniform3f(glGetUniformLocation(pid, "uSunlight"), mSky.sunlight.x, mSky.sunlight.y, mSky.sunlight.z);
    glm::vec3 rim = (mSky.skyHorizon * 0.9f + mSky.sunlight * 0.25f + mSky.ambient * 2.0f) * 1.5f;
    glUniform3f(glGetUniformLocation(pid, "uRimColor"), rim.x, rim.y, rim.z);

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
    if (mState == GameState::Won) {
        mHud.rect(0, 0, 1, 1, glm::vec4(0.10f, 0.15f, 0.10f, 0.55f));
        mHud.textCentered(0.5f, 0.34f, 0.07f, "THE GROVE AWAKENS", glm::vec4(0.8f, 1.0f, 0.7f, 1.0f));
        std::snprintf(buf, sizeof(buf), "FIREFLIES GATHERED  %d / %d", std::min(mCollected, mTarget), mTarget);
        mHud.textCentered(0.5f, 0.46f, 0.040f, buf, warm);
        mHud.textCentered(0.5f, 0.56f, 0.035f, "PRESS [R] TO PLAY AGAIN", warm);
    }
    if (mState == GameState::Lost) {
        mHud.rect(0, 0, 1, 1, glm::vec4(0.0f, 0.0f, 0.02f, 0.85f));
        mHud.textCentered(0.5f, 0.36f, 0.07f, "LOST IN THE DARK", glm::vec4(0.6f, 0.6f, 0.85f, 1.0f));
        mHud.textCentered(0.5f, 0.48f, 0.034f, "YOUR LANTERN WENT COLD.", warm);
        mHud.textCentered(0.5f, 0.56f, 0.034f, "PRESS [R] TO TRY AGAIN", warm);
    }

    mHud.end();
}

void Window::render() {
    renderGeometryPass();
    renderSSAO();
    renderSky();
    renderLightingPass();
    renderWater();
    renderGodrays();
    renderParticles();
    renderBloom();
    renderComposite();
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
