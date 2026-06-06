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

    // Procedural trees + rocks (all coded) scattered across the bowl, sat on the land.
    Player *props = new Player();
    props->setGeometry(proc::makeGroveProps(20260606u, 220, 120, 130.0f, 760.0f, ilo::terrainHeight));
    w.mProps = props;

    // The Heart of the Grove: a glowing idol floating above the centre of the Mere.
    Player *heart = new Player("./shapes/suzanne.obj");
    heart->setScale(glm::vec3(0.9f, 0.9f, 0.9f));
    heart->setTransform(0.0f, 2.4f, 0.0f, 0.0f);
    heart->setEmissive(glm::vec4(1.0f, 0.25f, 0.05f, 0.15f));
    w.addNPC(*heart);
    w.mHeart = heart;

    // A few deer quietly wandering the meadows.
    Player *deer1 = new Player("./shapes/Deer1.obj");
    deer1->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer1, -150.0f, 90.0f);

    Player *deer2 = new Player("./shapes/Deer1.obj");
    deer2->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer2, 130.0f, -120.0f);

    Player *deer3 = new Player("./shapes/Deer1.obj");
    deer3->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer3, 60.0f, 200.0f);

    w.loadGeometries();
    w.initAssets();
    w.run();

    delete forest;
    delete props;
    delete heart;
    delete deer1;
    delete deer2;
    delete deer3;
    return 0;
}
