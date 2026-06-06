#include "Window.h"
#include <vector>

int main(int argc, char *argv[]) {
    Window w(1280, 720, "Project Ilo - Firefly Grove");
    w.setvSync(false);
    w.setMouseSensitivity(0.0016f);
    w.setFOV(1.4f);

    // Spawn at the southern edge of the grove, looking north into the trees.
    Camera c1(glm::vec3(0, 2, 18), glm::vec3(0, 0, -1));
    w.setCamera(c1);

    // The forest: tile the miniforest patch into a 3x3 grove.
    Player *forest = new Player("./shapes/miniforest.obj");
    forest->setGrassWave(1.0f);
    std::vector<glm::vec3> offs;
    std::vector<glm::vec4> tints;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            offs.push_back(glm::vec3(i * 15.0f, 0.0f, j * 13.0f));
            tints.push_back(glm::vec4(1, 1, 1, 0));
        }
    }
    forest->setInstances(offs, tints);
    w.addNPC(*forest);
    w.mForest = forest;

    // A couple of deer quietly inhabiting the grove.
    Player *deer1 = new Player("./shapes/Deer1.obj");
    deer1->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    deer1->setTransform(-3.0f, 0.0f, -4.0f, 0.6f);
    w.addNPC(*deer1);

    Player *deer2 = new Player("./shapes/Deer1.obj");
    deer2->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    deer2->setTransform(5.0f, 0.0f, -9.0f, -1.2f);
    w.addNPC(*deer2);

    w.loadGeometries();
    w.initAssets();
    w.run();

    delete forest;
    delete deer1;
    delete deer2;
    return 0;
}
