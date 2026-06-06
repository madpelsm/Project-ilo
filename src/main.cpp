#include "Props.h"
#include "Window.h"
#include <cmath>
#include <vector>

int main(int argc, char *argv[]) {
    Window w(1280, 720, "Project Ilo - Lumenmere");
    w.setvSync(false);
    w.setMouseSensitivity(0.0016f);
    w.setFOV(1.4f);

    // Spawn on the southern shore of the Mere, looking north across the water.
    Camera c1(glm::vec3(0, 5, 160), glm::vec3(0, 0, -1));
    w.setCamera(c1);

    // Groves of the existing low-poly forest, scattered around the meadow ring and
    // sat on the terrain (the centre is the lake, so we keep the shore clear).
    Player *forest = new Player("./shapes/miniforest.obj");
    forest->setGrassWave(1.0f);
    std::vector<glm::vec3> offs;
    std::vector<glm::vec4> tints;
    for (int k = 0; k < 10; k++) {
        float ang = k * 0.6283f + 0.3f;
        float rad = 180.0f + 30.0f * (k % 3);
        float x = std::cos(ang) * rad, z = std::sin(ang) * rad;
        offs.push_back(glm::vec3(x, ilo::terrainHeight(x, z), z));
        tints.push_back(glm::vec4(1, 1, 1, 0));
    }
    forest->setInstances(offs, tints);
    w.addNPC(*forest);
    w.mForest = forest;

    // (Trees, grass, flowers and rocks are scattered as instanced fields by the engine.)

    // The Heart of the Grove: a glowing idol floating above the centre of the Mere.
    Player *heart = new Player("./shapes/suzanne.obj");
    heart->setScale(glm::vec3(0.9f, 0.9f, 0.9f));
    heart->setTransform(0.0f, 2.4f, 0.0f, 0.0f);
    heart->setEmissive(glm::vec4(1.0f, 0.25f, 0.05f, 0.15f));
    w.addNPC(*heart);
    w.mHeart = heart;

    // Two herds of deer grazing the meadows — one on the near south shore (in view at
    // spawn), one across the Mere on the far shore, so the world feels inhabited from the
    // first moment. They wander as cohesive groups (see Window::updateDeer).
    // yawOffset corrects Deer1.obj's modelled forward axis so they face where they walk.
    const float DEER_YAW = 3.14159265f; // model faces -Z; rotate 180° to face heading
    std::vector<Player *> deer;
    auto spawnDeer = [&](float x, float z, int herd) {
        Player *d = new Player("./shapes/Deer1.obj");
        d->setScale(glm::vec3(1.35f, 1.35f, 1.35f));
        w.addDeer(*d, x, z, herd, DEER_YAW);
        deer.push_back(d);
    };
    // Herd 0 — the south-east meadow (player's right as they face the lake): solid land
    // on the rising ring, easy to wander into early.
    spawnDeer(180.0f, 175.0f, 0);
    spawnDeer(215.0f, 150.0f, 0);
    spawnDeer(160.0f, 205.0f, 0);
    spawnDeer(140.0f, 165.0f, 0);
    // Herd 1 — the far north shore, grazing in view across the water at spawn.
    spawnDeer(-44.0f, -210.0f, 1);
    spawnDeer(6.0f, -232.0f, 1);
    spawnDeer(54.0f, -205.0f, 1);

    w.loadGeometries();
    w.initAssets();
    w.run();

    delete forest;
    delete heart;
    for (Player *d : deer)
        delete d;
    return 0;
}
