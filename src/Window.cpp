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
    if (closed)
        return;
    closed = true;
    std::cout << "killing SDL" << std::endl;
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->cleanup();
    }
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
    bool sprint = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    float speed = (sprint ? SPRINT_SPEED : WALK_SPEED) * mDt;
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

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEMOTION:
            mCamera.rotate(-mMouseSensitivity * event.motion.xrel, -mMouseSensitivity * event.motion.yrel);
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
    float f = mFuel;
    ilo::OmniLightGPU lantern;
    lantern.posRadius[0] = lp.x;
    lantern.posRadius[1] = lp.y;
    lantern.posRadius[2] = lp.z;
    lantern.posRadius[3] = 3.0f + 9.0f * f;
    glm::vec3 lc(1.00f, 0.72f, 0.42f);
    float li = 0.6f + 2.4f * f;
    lantern.colorIntensity[0] = lc.x;
    lantern.colorIntensity[1] = lc.y;
    lantern.colorIntensity[2] = lc.z;
    lantern.colorIntensity[3] = li;
    mLights.push_back(lantern);

    // (firefly + Heart lights are appended in later slices)

    lightUBO.upload(mLights.data(), (int)mLights.size());
}

void Window::update() {
    packLights();
    for (unsigned int i = 0; i < mGameObjects.size(); i++) {
        mGameObjects[i]->update();
    }
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
    // implemented in the bloom slice; for now bloom stays disabled
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
    glBindTexture(GL_TEXTURE_2D, mBlackTex); // no bloom yet
    glUniform1f(glGetUniformLocation(pid, "uExposure"), mExposure);
    glUniform1f(glGetUniformLocation(pid, "uBloomIntensity"), 0.0f);
    glUniform1f(glGetUniformLocation(pid, "uVignetteMax"), mVignetteMax);
    glUniform1f(glGetUniformLocation(pid, "uFuel"), mFuel);
    tri.draw();
}

void Window::render() {
    renderGeometryPass();
    renderLightingPass();
    renderBloom();
    renderComposite();

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
