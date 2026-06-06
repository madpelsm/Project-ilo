#include "Window.h"

#include "Screenshot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
// Movement tuning (metres / second), from the design spec.
const float WALK_SPEED = 4.0f;
const float SPRINT_SPEED = 6.5f;
const float FLY_VERT_SPEED = 3.0f;
const float HARD_BOUND = 24.0f; // clamp camera to the play area
const float DT_CLAMP = 0.05f;
const float COLLECT_RADIUS = 1.8f;

// Fuel / warmth tuning (warmth units), from the design spec.
const float FUEL_MAX = 100.0f;
const float FUEL_START = 70.0f;
const float FUEL_DRAIN = 2.5f;
const float SPRINT_DRAIN_EXTRA = 1.5f;
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
    SDL_DestroyWindow(mSDLwindow);
    SDL_Quit();
}

void Window::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "failed to intialise video" << std::endl;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    mSDLwindow = SDL_CreateWindow(
        mTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mWidth, mHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        printf("Something went wrong!\n");
        exit(-1);
    }
    printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);

    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);

    buildProgram(geometryProg, "./shaders/firstPassVertex.vert", "./shaders/firstPassFragment.frag");
    buildProgram(lightingProg, "./shaders/fullscreen.vert", "./shaders/secondPassFrag.frag");
    buildProgram(brightProg, "./shaders/fullscreen.vert", "./shaders/bloomBright.frag");
    buildProgram(blurProg, "./shaders/fullscreen.vert", "./shaders/bloomBlur.frag");
    buildProgram(compositeProg, "./shaders/fullscreen.vert", "./shaders/thirdPassFrag.frag");

    // Static sampler bindings.
    lightingProg.useProgram();
    GLuint lp = lightingProg.getProgramID();
    glUniform1i(glGetUniformLocation(lp, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lp, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lp, "gAlbedo"), 2);
    glUniform1i(glGetUniformLocation(lp, "gMtlProps"), 3);
    glUniformBlockBinding(lp, glGetUniformBlockIndex(lp, "LightBlock"), 0);

    brightProg.useProgram();
    glUniform1i(glGetUniformLocation(brightProg.getProgramID(), "uScene"), 0);
    blurProg.useProgram();
    glUniform1i(glGetUniformLocation(blurProg.getProgramID(), "image"), 0);
    compositeProg.useProgram();
    glUniform1i(glGetUniformLocation(compositeProg.getProgramID(), "uScene"), 0);
    glUniform1i(glGetUniformLocation(compositeProg.getProgramID(), "uBloom"), 1);

    tri.init();
    lightUBO.init();
    mHud.init();

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

    // HDR lighting target (linear, filtered for bloom sampling).
    hdrFBO.create(mWidth, mHeight);
    hdrFBO.addColor(GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Window::destroyFramebuffers() {
    gBuffer.destroy();
    hdrFBO.destroy();
    bloomA.destroy();
    bloomB.destroy();
    mBloomReady = false;
}

void Window::loadGeometries() {
    std::vector<std::thread> loaders;
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        loaders.push_back(std::thread(&Player::loadDefaultGeometry, mGameObjects[i]));
    }
    for (unsigned int i = 0; i < loaders.size(); i++) {
        loaders[i].join();
    }
}

void Window::initAssets() {
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->initGL();
    }
    mFireflies.init(60);
}

void Window::run() {
    if (!windowInitialised) {
        std::cout << "Window not initialised" << std::endl;
        return;
    }

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
        render();

        if (shotPath && ++frame >= shotFrame) {
            ilo::savePPM(shotPath, mWidth, mHeight);
            std::cout << "Saved screenshot to " << shotPath << " (" << mWidth << "x" << mHeight << ")" << std::endl;
            break;
        }
    }
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
        if (state[SDL_SCANCODE_SPACE])
            mCamera.moveUp(FLY_VERT_SPEED * mDt);
        if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_C])
            mCamera.moveUp(-FLY_VERT_SPEED * mDt);
    }

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEMOTION:
            mCamera.rotate(-mMouseSensitivity * event.motion.xrel, -mMouseSensitivity * event.motion.yrel);
            break;
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

    // keep the camera inside the grove
    mCamera.mPosition.x = std::max(-HARD_BOUND, std::min(HARD_BOUND, mCamera.mPosition.x));
    mCamera.mPosition.z = std::max(-HARD_BOUND, std::min(HARD_BOUND, mCamera.mPosition.z));
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
    // Low-warmth stutter: the dying lantern flickers and briefly cuts out.
    if (f < 0.20f) {
        float flicker = 1.0f + 0.15f * std::sin(6.2831f * 8.0f * mTime);
        float dropout = (std::fmod(mTime, 2.0f) < 0.1f) ? 0.3f : 1.0f;
        li *= flicker * dropout;
    }
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
        float intensity = glm::mix(0.3f, 5.0f, p);
        if (mState == GameState::Won)
            intensity = 6.0f + 1.5f * std::sin(6.2831f * 0.5f * mTime);
        heart.colorIntensity[0] = mHeartColor.x;
        heart.colorIntensity[1] = mHeartColor.y;
        heart.colorIntensity[2] = mHeartColor.z;
        heart.colorIntensity[3] = intensity;
        mLights.push_back(heart);
    }

    // Firefly lights (nearest motes to the camera), capped to keep the loop cheap.
    int cap = mLowSpec ? 14 : 46;
    mFireflies.appendLights(mLights, mTime, mCamera.mPosition, cap);

    lightUBO.upload(mLights.data(), (int)mLights.size());
}

void Window::update() {
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
            if (mPulses.size() < 8)
                mPulses.push_back({e.pos, e.color, 0.0f, 0.35f});
            mFlash = std::min(0.25f, mFlash + 0.15f);
        }
        if (!mFreezeFuel) {
            float drain = FUEL_DRAIN + (mSprinting ? SPRINT_DRAIN_EXTRA : 0.0f);
            mFuelW -= drain * mDt;
        }
        if (mFuelW <= 0.0f) {
            mFuelW = 0.0f;
            mState = GameState::Lost;
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
    mHeartEmissive = glm::mix(0.15f, 6.0f, p);
    if (mState == GameState::Won) // breathe when fully ablaze
        mHeartEmissive = 7.0f + 1.0f * std::sin(6.2831f * 0.5f * mTime);
    if (mHeart)
        mHeart->setEmissive(glm::vec4(mHeartColor, mHeartEmissive));
}

void Window::addDeer(Player &deer, float x, float z) {
    addNPC(deer);
    DeerAgent d;
    d.p = &deer;
    d.x = x;
    d.z = z;
    d.tx = x;
    d.tz = z;
    d.pause = 1.0f + 3.0f * ((x * 13.0f + z * 7.0f) - std::floor(x * 13.0f + z * 7.0f));
    mDeer.push_back(d);
    deer.setTransform(x, 0.0f, z, 0.0f);
}

void Window::updateDeer() {
    if (mState != GameState::Playing && mState != GameState::Intro)
        return;
    for (DeerAgent &d : mDeer) {
        float dx = d.tx - d.x, dz = d.tz - d.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < 0.2f) {
            d.pause -= mDt;
            if (d.pause <= 0.0f) {
                // pick a new wander target within the grove
                float a = std::sin(mTime * 1.3f + d.x * 2.1f + d.z) * 43758.5453f;
                a = a - std::floor(a);
                float b = std::sin(mTime * 0.7f + d.z * 1.7f) * 12543.1234f;
                b = b - std::floor(b);
                float ang = a * 6.2831f;
                float rad = 4.0f + 8.0f * b;
                d.tx = std::max(-18.0f, std::min(18.0f, d.x + std::cos(ang) * rad));
                d.tz = std::max(-18.0f, std::min(18.0f, d.z + std::sin(ang) * rad));
                d.pause = 2.5f + 3.5f * a;
            }
        } else {
            float step = DEER_SPEED * mDt;
            d.x += dx / dist * step;
            d.z += dz / dist * step;
            d.yaw = std::atan2(dx, dz);
        }
        if (d.p)
            d.p->setTransform(d.x, 0.0f, d.z, 0.0f);
    }
}

void Window::resetGame() {
    mFuelW = FUEL_START;
    mCollected = 0;
    mState = GameState::Intro;
    mIntroTimer = 1.5f;
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
    glm::mat4 persp = glm::perspective(mFOV, mWidth / (float)mHeight, 0.1f, 200.0f);
    glUniformMatrix4fv(glGetUniformLocation(pid, "persp"), 1, GL_FALSE, glm::value_ptr(persp));
    glUniformMatrix4fv(glGetUniformLocation(pid, "view"), 1, GL_FALSE, glm::value_ptr(mCamera.mView));
    glUniform1f(glGetUniformLocation(pid, "time"), mTime);

    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->render(pid);
    }
    mFireflies.render(pid, mTime); // disables cull internally; drawn last
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

    glm::mat4 persp = glm::perspective(mFOV, mWidth / (float)mHeight, 0.1f, 200.0f);
    glm::mat4 invVP = glm::inverse(persp * mCamera.mView);
    glUniformMatrix4fv(glGetUniformLocation(pid, "invViewProj"), 1, GL_FALSE, glm::value_ptr(invVP));
    glUniform3f(glGetUniformLocation(pid, "eyePos"), mCamera.mPosition.x, mCamera.mPosition.y, mCamera.mPosition.z);
    glUniform3f(glGetUniformLocation(pid, "uAmbient"), mAmbient.x, mAmbient.y, mAmbient.z);
    glUniform3f(glGetUniformLocation(pid, "uFogColor"), mFogColor.x, mFogColor.y, mFogColor.z);
    glUniform1f(glGetUniformLocation(pid, "uFogDensity"), mFogDensity);
    glUniform1f(glGetUniformLocation(pid, "uFogHeightFalloff"), mFogHeightFalloff);
    glUniform1f(glGetUniformLocation(pid, "uFogBaseY"), mFogBaseY);
    glUniform3f(glGetUniformLocation(pid, "uSkyTop"), mSkyTop.x, mSkyTop.y, mSkyTop.z);
    glUniform3f(glGetUniformLocation(pid, "uSkyHorizon"), mSkyHorizon.x, mSkyHorizon.y, mSkyHorizon.z);
    glUniform3f(glGetUniformLocation(pid, "uMoonDir"), mMoonDir.x, mMoonDir.y, mMoonDir.z);
    glUniform3f(glGetUniformLocation(pid, "uMoonColor"), mMoonColor.x, mMoonColor.y, mMoonColor.z);
    glUniform1f(glGetUniformLocation(pid, "uMoonSize"), mMoonSize);

    lightUBO.bindBase(0);
    tri.draw();
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

    // Firefly counter (top-left).
    std::snprintf(buf, sizeof(buf), "FIREFLIES  %d / %d", mCollected, mTarget);
    mHud.text(0.04f, 0.05f, 0.040f, buf, warm);

    // Warmth meter (bottom-left).
    mHud.text(0.04f, 0.860f, 0.026f, "WARMTH", warm);
    mHud.rect(0.04f, 0.90f, 0.30f, 0.035f, glm::vec4(0.05f, 0.05f, 0.07f, 0.6f));
    glm::vec4 fill = f > 0.5f ? glm::vec4(1.0f, 0.75f, 0.4f, 1.0f)
                              : (f > 0.25f ? glm::vec4(1.0f, 0.5f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.2f, 0.1f, 1.0f));
    if (f <= 0.25f)
        fill.a *= 0.6f + 0.4f * std::sin(6.2831f * 2.0f * mTime);
    if (f > 0.001f)
        mHud.rect(0.04f, 0.90f, 0.30f * f, 0.035f, fill);

    if (mState == GameState::Playing && f <= 0.20f && ((int)(mTime * 2.0f) % 2 == 0))
        mHud.textCentered(0.5f, 0.12f, 0.035f, "FIND A FIREFLY", glm::vec4(1.0f, 0.25f, 0.2f, 1.0f));

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
        std::snprintf(buf, sizeof(buf), "FIREFLIES GATHERED  %d / %d", mCollected, mTarget);
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
    renderLightingPass();
    renderBloom();
    renderComposite();
    renderHud();

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
