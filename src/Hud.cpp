#include "Hud.h"
#include "Font8x8.h"
#include "Shader.h"

static const int AW = 128; // atlas: 16 cols x 6 rows of 8x8 glyphs
static const int AH = 48;

void Hud::init() {
    // Build the font atlas (R8): glyph g at cell (g%16, g/16), row 0 = glyph top.
    std::vector<unsigned char> atlas(AW * AH, 0);
    for (int g = 0; g < 96; ++g) {
        int cx = (g % 16) * 8;
        int cy = (g / 16) * 8;
        for (int gr = 0; gr < 8; ++gr) {
            unsigned char bits = FONT8X8[g][gr];
            for (int gc = 0; gc < 8; ++gc) {
                if ((bits >> gc) & 1) {
                    int ax = cx + gc;
                    int ay = cy + gr;
                    atlas[ay * AW + ax] = 255;
                }
            }
        }
    }
    glGenTextures(1, &mFontTex);
    glBindTexture(GL_TEXTURE_2D, mFontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    unsigned char white = 255;
    glGenTextures(1, &mWhiteTex);
    glBindTexture(GL_TEXTURE_2D, mWhiteTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    Shader vs, fs;
    vs.loadShader("./shaders/hud.vert", GL_VERTEX_SHADER);
    fs.loadShader("./shaders/hud.frag", GL_FRAGMENT_SHADER);
    mProg.createProgram();
    mProg.attachShaderToProgram(&vs);
    mProg.attachShaderToProgram(&fs);
    mProg.linkProgram();
    vs.deleteShader();
    fs.deleteShader();
    mProg.useProgram();
    glUniform1i(glGetUniformLocation(mProg.getProgramID(), "uTex"), 0);

    glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);
    glGenBuffers(1, &mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(4 * sizeof(float)));
    glBindVertexArray(0);
}

void Hud::destroy() {
    if (mVao)
        glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    if (mFontTex)
        glDeleteTextures(1, &mFontTex);
    if (mWhiteTex)
        glDeleteTextures(1, &mWhiteTex);
    mVao = mVbo = mFontTex = mWhiteTex = 0;
}

void Hud::begin(int screenW, int screenH) {
    mW = screenW > 0 ? screenW : 1;
    mH = screenH > 0 ? screenH : 1;
    mSolid.clear();
    mText.clear();
}

void Hud::pushQuad(std::vector<V> &buf, float x, float y, float w, float h,
                   float u0, float v0, float u1, float v1, glm::vec4 c) {
    // normalized top-left -> NDC
    float x0 = x * 2.0f - 1.0f, x1 = (x + w) * 2.0f - 1.0f;
    float y0 = 1.0f - y * 2.0f, y1 = 1.0f - (y + h) * 2.0f;
    V tl{x0, y0, u0, v0, c.r, c.g, c.b, c.a};
    V tr{x1, y0, u1, v0, c.r, c.g, c.b, c.a};
    V bl{x0, y1, u0, v1, c.r, c.g, c.b, c.a};
    V br{x1, y1, u1, v1, c.r, c.g, c.b, c.a};
    buf.push_back(tl);
    buf.push_back(bl);
    buf.push_back(br);
    buf.push_back(tl);
    buf.push_back(br);
    buf.push_back(tr);
}

void Hud::rect(float x, float y, float w, float h, glm::vec4 color) {
    pushQuad(mSolid, x, y, w, h, 0.5f, 0.5f, 0.5f, 0.5f, color);
}

float Hud::textWidth(const std::string &s, float height) const {
    float charW = height * (float)mH / (float)mW;
    return s.size() * charW;
}

void Hud::text(float x, float y, float height, const std::string &s, glm::vec4 color) {
    float charW = height * (float)mH / (float)mW; // keep glyph pixels square
    float cx = x;
    for (char ch : s) {
        int g = (int)(unsigned char)ch - 32;
        if (g >= 0 && g < 96 && ch != ' ') {
            int gx = (g % 16) * 8;
            int gy = (g / 16) * 8;
            float u0 = gx / (float)AW, u1 = (gx + 8) / (float)AW;
            float v0 = gy / (float)AH, v1 = (gy + 8) / (float)AH;
            pushQuad(mText, cx, y, charW, height, u0, v0, u1, v1, color);
        }
        cx += charW;
    }
}

void Hud::textCentered(float cx, float y, float height, const std::string &s, glm::vec4 color) {
    text(cx - textWidth(s, height) * 0.5f, y, height, s, color);
}

void Hud::drawBatch(std::vector<V> &buf, GLuint tex) {
    if (buf.empty())
        return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(V), nullptr, GL_STREAM_DRAW);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(V), buf.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)buf.size());
    glBindVertexArray(0);
}

void Hud::end() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mProg.useProgram();
    drawBatch(mSolid, mWhiteTex);
    drawBatch(mText, mFontTex);
    glDisable(GL_BLEND);
}
