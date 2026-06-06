#include "InstancedField.h"
#include <algorithm>

// Set up the VAO with the asset mesh (attribs 0-3) and empty instance buffers
// (attribs 4-6, divisor 1) sized for reserveInstances with the given usage hint.
void InstancedField::setupMesh(const std::vector<Vertex2> &mesh, GLenum instanceUsage, int reserveInstances) {
    mIndexCount = (int)mesh.size();
    glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);

    glGenBuffers(1, &mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(Vertex2), mesh.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)sizeof(glm::vec3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(2 * sizeof(glm::vec3)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(3 * sizeof(glm::vec3)));

    glGenBuffers(1, &mOffVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mOffVbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)reserveInstances * sizeof(glm::vec3), nullptr, instanceUsage);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glVertexAttribDivisor(4, 1);

    glGenBuffers(1, &mTintVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mTintVbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)reserveInstances * sizeof(glm::vec4), nullptr, instanceUsage);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(5, 1);

    glGenBuffers(1, &mXformVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mXformVbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)reserveInstances * sizeof(glm::vec4), nullptr, instanceUsage);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(6, 1);

    glGenBuffers(1, &mIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    std::vector<GLuint> idx(mIndexCount);
    for (int i = 0; i < mIndexCount; i++)
        idx[i] = i;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

static void uploadInstances(GLuint off, GLuint tint, GLuint xform, const std::vector<FieldInstance> &inst) {
    std::vector<glm::vec3> offs(inst.size());
    std::vector<glm::vec4> tints(inst.size()), xforms(inst.size());
    for (size_t i = 0; i < inst.size(); i++) {
        offs[i] = inst[i].pos;
        tints[i] = inst[i].tintEmissive;
        xforms[i] = inst[i].xform;
    }
    glBindBuffer(GL_ARRAY_BUFFER, off);
    glBufferSubData(GL_ARRAY_BUFFER, 0, offs.size() * sizeof(glm::vec3), offs.data());
    glBindBuffer(GL_ARRAY_BUFFER, tint);
    glBufferSubData(GL_ARRAY_BUFFER, 0, tints.size() * sizeof(glm::vec4), tints.data());
    glBindBuffer(GL_ARRAY_BUFFER, xform);
    glBufferSubData(GL_ARRAY_BUFFER, 0, xforms.size() * sizeof(glm::vec4), xforms.data());
}

void InstancedField::build(const std::vector<Vertex2> &mesh, const std::vector<FieldInstance> &instances) {
    destroy();
    if (mesh.empty() || instances.empty())
        return;
    mMax = mCount = (int)instances.size();
    setupMesh(mesh, GL_STATIC_DRAW, mMax);
    uploadInstances(mOffVbo, mTintVbo, mXformVbo, instances);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedField::buildDynamic(const std::vector<Vertex2> &mesh, int maxInstances) {
    destroy();
    if (mesh.empty() || maxInstances <= 0)
        return;
    mMax = maxInstances;
    mCount = 0;
    setupMesh(mesh, GL_DYNAMIC_DRAW, mMax);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedField::update(const std::vector<FieldInstance> &instances) {
    if (!mVao)
        return;
    mCount = std::min((int)instances.size(), mMax);
    if (mCount <= 0)
        return;
    std::vector<FieldInstance> clipped(instances.begin(), instances.begin() + mCount);
    uploadInstances(mOffVbo, mTintVbo, mXformVbo, clipped);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedField::render(int) {
    if (!mVao || mCount <= 0)
        return;
    glBindVertexArray(mVao);
    glDrawElementsInstanced(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0, mCount);
    glBindVertexArray(0);
}

void InstancedField::destroy() {
    if (mVao)
        glDeleteVertexArrays(1, &mVao);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mIbo);
    glDeleteBuffers(1, &mOffVbo);
    glDeleteBuffers(1, &mTintVbo);
    glDeleteBuffers(1, &mXformVbo);
    mVao = mVbo = mIbo = mOffVbo = mTintVbo = mXformVbo = 0;
    mIndexCount = mCount = mMax = 0;
}
