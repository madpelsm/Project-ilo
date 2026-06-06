#include "Props.h"
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

    // The forest: tile the miniforest patch into a 3x3 grove, leaving the centre
    // tile open as a glade for the Heart of the Grove.
    Player *forest = new Player("./shapes/miniforest.obj");
    forest->setGrassWave(1.0f);
    std::vector<glm::vec3> offs;
    std::vector<glm::vec4> tints;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0)
                continue; // clearing for the Heart
            offs.push_back(glm::vec3(i * 15.0f, 0.0f, j * 13.0f));
            tints.push_back(glm::vec4(1, 1, 1, 0));
        }
    }
    forest->setInstances(offs, tints);
    w.addNPC(*forest);
    w.mForest = forest;

    // Procedurally generated trees + rocks (all coded, no asset files) thicken the
    // grove and wrap it in a denser ring of woodland so the edges fade into the dark.
    Player *props = new Player();
    props->setGeometry(proc::makeGroveProps(20260606u, 90, 55, 9.0f, 23.0f));
    w.mProps = props;

    // The Heart of the Grove: a glowing idol at the origin that wakes as fireflies
    // are gathered. Its emissive colour/strength is driven each frame by the game.
    Player *heart = new Player("./shapes/suzanne.obj");
    heart->setScale(glm::vec3(0.9f, 0.9f, 0.9f));
    heart->setTransform(0.0f, 1.6f, 0.0f, 0.0f);
    heart->setEmissive(glm::vec4(1.0f, 0.25f, 0.05f, 0.15f));
    w.addNPC(*heart);
    w.mHeart = heart;

    // A few deer quietly wandering the grove.
    Player *deer1 = new Player("./shapes/Deer1.obj");
    deer1->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer1, -6.0f, -4.0f);

    Player *deer2 = new Player("./shapes/Deer1.obj");
    deer2->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer2, 7.0f, -9.0f);

    Player *deer3 = new Player("./shapes/Deer1.obj");
    deer3->setScale(glm::vec3(0.8f, 0.8f, 0.8f));
    w.addDeer(*deer3, 2.0f, 8.0f);

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
