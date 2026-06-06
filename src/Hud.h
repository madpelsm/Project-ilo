#pragma once
// Minimal 2D overlay: alpha-blended bars and bitmap text drawn after the final
// composite. Coordinates are normalized with a top-left origin (0,0)..(1,1) so the
// HUD rescales automatically on resize. Text uses an embedded 8x8 font atlas.
#include "ShaderProgram.h"
#include "GL.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Hud {
  public:
    void init();
    void destroy();

    void begin(int screenW, int screenH);
    void rect(float x, float y, float w, float h, glm::vec4 color); // solid bar
    void text(float x, float y, float height, const std::string &s, glm::vec4 color);
    void textCentered(float cx, float y, float height, const std::string &s, glm::vec4 color);
    float textWidth(const std::string &s, float height) const; // normalized width
    void end(); // upload + draw (solids, then text)

  private:
    struct V {
        float x, y, u, v, r, g, b, a;
    };
    std::vector<V> mSolid, mText;
    GLuint mVao = 0, mVbo = 0, mFontTex = 0, mWhiteTex = 0;
    ShaderProgram mProg;
    int mW = 1, mH = 1;

    void pushQuad(std::vector<V> &buf, float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1, glm::vec4 c);
    void drawBatch(std::vector<V> &buf, GLuint tex);
};
