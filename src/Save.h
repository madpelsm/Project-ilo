#pragma once
// "The vale remembers." A tiny, versioned, dependency-free save blob for LUMENMERE:
// what light you gave the world (radiance, collected, woken beacons, woven
// constellations, whether the Long Dawn broke, the Mere's seed). Pure serialize/
// deserialize over a keyed text format — the platform byte sink (native file vs web
// localStorage) lives in Window.cpp. Forward-tolerant: unknown lines are ignored, a
// missing key keeps its default, and a bad magic/version fails safe to a fresh vale.
#include <glm/glm.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace ilo {

static const int SAVE_VERSION = 1;
static const int SAVE_MAX_BYTES = 8192;

struct SavedConstellation {
    glm::vec3 color = glm::vec3(1.0f);
    std::vector<glm::vec3> stars;  // unit sky directions
    std::vector<glm::vec3> ground; // absolute world marker positions
};

struct SaveData {
    int version = SAVE_VERSION;
    float radiance = 0.0f;
    int collected = 0;
    int beaconCount = 0;
    std::vector<int> beaconLit; // 0/1 per beacon
    std::vector<SavedConstellation> cons;
    bool longDawn = false;
    int seedsPlanted = 0;
    bool seedPresent = false;
};

inline std::string serialize(const SaveData &s) {
    std::ostringstream o;
    o << "LUMENMERE " << SAVE_VERSION << "\n";
    o << "R " << s.radiance << "\n";
    o << "C " << s.collected << "\n";
    o << "B " << s.beaconCount;
    for (int b : s.beaconLit)
        o << " " << b;
    o << "\n";
    o << "K " << (int)s.cons.size() << "\n";
    for (const SavedConstellation &c : s.cons) {
        o << "X " << c.color.x << " " << c.color.y << " " << c.color.z << " " << (int)c.stars.size() << "\n";
        for (const glm::vec3 &d : c.stars)
            o << "S " << d.x << " " << d.y << " " << d.z << "\n";
        for (const glm::vec3 &g : c.ground)
            o << "G " << g.x << " " << g.y << " " << g.z << "\n";
    }
    o << "D " << (s.longDawn ? 1 : 0) << "\n";
    o << "P " << s.seedsPlanted << " " << (s.seedPresent ? 1 : 0) << "\n";
    return o.str();
}

// Returns false (and leaves out with defaults) if the blob is empty / wrong magic /
// wrong version — the caller then simply starts a fresh vale.
inline bool deserialize(const std::string &blob, SaveData &out) {
    std::istringstream in(blob);
    std::string magic;
    int ver = 0;
    if (!(in >> magic >> ver) || magic != "LUMENMERE" || ver != SAVE_VERSION)
        return false;
    out = SaveData();
    std::string key;
    SavedConstellation *cur = nullptr; // the constellation currently being filled
    int curStarsExpected = 0;
    while (in >> key) {
        if (key == "R") {
            in >> out.radiance;
        } else if (key == "C") {
            in >> out.collected;
        } else if (key == "B") {
            in >> out.beaconCount;
            out.beaconLit.assign(std::max(0, out.beaconCount), 0);
            for (int i = 0; i < out.beaconCount; i++)
                in >> out.beaconLit[i];
        } else if (key == "K") {
            int n = 0;
            in >> n;
            out.cons.clear();
            cur = nullptr;
        } else if (key == "X") {
            SavedConstellation c;
            in >> c.color.x >> c.color.y >> c.color.z >> curStarsExpected;
            out.cons.push_back(c);
            cur = &out.cons.back();
        } else if (key == "S" && cur) {
            glm::vec3 d;
            in >> d.x >> d.y >> d.z;
            cur->stars.push_back(d);
        } else if (key == "G" && cur) {
            glm::vec3 g;
            in >> g.x >> g.y >> g.z;
            cur->ground.push_back(g);
        } else if (key == "D") {
            int d = 0;
            in >> d;
            out.longDawn = d != 0;
        } else if (key == "P") {
            int sp = 0, present = 0;
            in >> sp >> present;
            out.seedsPlanted = sp;
            out.seedPresent = present != 0;
        } else {
            std::getline(in, key); // unknown key: skip the rest of the line, stay tolerant
        }
    }
    return true;
}

} // namespace ilo
