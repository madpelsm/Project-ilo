#include "Player.h"
#include <SDL2/SDL.h>
Player::Player() {
    mOffsets.push_back(glm::vec3(0, 0, 0));
    mTintEmissive.push_back(glm::vec4(1, 1, 1, 0));
}
Player::~Player() {
    cleanup();
}

void Player::loadGeometry(std::string filePath) {
    // gets Vertex2 objects, so it contains normals
    objectLoader objLoader(filePath);
    mVertices2 = objLoader.getVertices();
    std::cout << "Player Geometry loaded" << std::endl;
    // createNormals();
    createIndices();
}

void Player::setTransform(float x, float y, float z, float angle) {
    // set the transformation info, then create the transformationMatrix
    mX = x;
    mY = y;
    mZ = z;
    mRotAngle = angle;
}

void Player::setShape(std::vector<Vertex> vertices) {
    // pass in a vector with vertex info (pos and color) do this with origin at
    // the center of the object
    mVertices = vertices;
    // create a new vector with all the normal information
    // createNormals();
}

void Player::createIndices() {
    for (unsigned int i = 0; i < mVertices2.size(); i += 3) {
        mIndices.push_back(i);
        mIndices.push_back(i + 1);
        mIndices.push_back(i + 2);
    }
    printf("created indices for player \n");
}

Player::Player(std::string _geomPath) {
    mOffsets.push_back(glm::vec3(0, 0, 0));
    mTintEmissive.push_back(glm::vec4(1, 1, 1, 0));
    mGeomPath = _geomPath;
}

void Player::createNormals() {
    // Create normals here
    // take segments of 3 vertices per iteration, add the calculated normal to the
    // normallist
    for (unsigned int i = 0; i < mVertices.size(); i += 3) {
        // on next loop if i was 0, i will start from 3. [3] [4] [5] ..
        Vertex tempVertex1 = mVertices[i];
        Vertex tempVertex2 = mVertices[i + 1];
        Vertex tempVertex3 = mVertices[i + 2];
        glm::vec3 a = tempVertex3.Pos - tempVertex1.Pos; // from vert 1 to vert 2
        glm::vec3 b = tempVertex2.Pos - tempVertex1.Pos; // from vert1 to vert 3
        glm::vec3 normal = glm::normalize(glm::cross(a, b));
        mVertices2.push_back(Vertex2(tempVertex1, normal, glm::vec3(32, 0.5f, 0.0f)));
        mVertices2.push_back(Vertex2(tempVertex2, normal, glm::vec3(32, 0.5f, 0.0f)));
        mVertices2.push_back(Vertex2(tempVertex3, normal, glm::vec3(32, 0.5f, 0.0f)));
    }
    std::cout << "normals Created for player" << std::endl;
}

void Player::setScale(glm::vec3 _scale) {
    mScale = _scale;
}

glm::vec3 Player::getPosition() {
    return glm::vec3(mX, mY, mZ);
}

void Player::addInstance(glm::vec3 _offset) {
    addInstance(_offset, glm::vec4(1, 1, 1, 0));
}

void Player::addInstance(glm::vec3 _offset, glm::vec4 _tintEmissive) {
    mOffsetsChanged = true;
    mOffsets.push_back(_offset);
    mTintEmissive.push_back(_tintEmissive);
}

void Player::setEmissive(glm::vec4 _tintEmissive) {
    if (mTintEmissive.empty())
        mTintEmissive.push_back(_tintEmissive);
    else
        mTintEmissive[0] = _tintEmissive;
    mOffsetsChanged = true;
}

void Player::setInstances(const std::vector<glm::vec3> &offsets, const std::vector<glm::vec4> &tints) {
    mOffsets = offsets;
    mTintEmissive = tints;
    mOffsetsChanged = true;
}

void Player::update() {
    mTransformation = glm::mat4(glm::translate(glm::mat4(1), glm::vec3(mX, mY, mZ)) *
                                glm::rotate(glm::mat4(1), mRotAngle, glm::vec3(0, 0, 1)) * glm::scale(glm::mat4(1), mScale));
}

void Player::render(int shaderProgramID) {
    if (mOffsets.empty())
        return;
    // Re-upload instance data when it changed (always, for the dynamic firefly swarm).
    if (mOffsetsChanged || mDynamic) {
        // Orphan then refill to avoid stalling on the previous frame's draw.
        glBindBuffer(GL_ARRAY_BUFFER, mInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, mOffsets.size() * sizeof(glm::vec3), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, mOffsets.size() * sizeof(glm::vec3), &mOffsets[0]);
        glBindBuffer(GL_ARRAY_BUFFER, mTintVBO);
        glBufferData(GL_ARRAY_BUFFER, mTintEmissive.size() * sizeof(glm::vec4), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, mTintEmissive.size() * sizeof(glm::vec4), &mTintEmissive[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        mOffsetsChanged = false;
    }
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(mTransformation)));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(mTransformation));
    glUniformMatrix3fv(glGetUniformLocation(shaderProgramID, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform1f(glGetUniformLocation(shaderProgramID, "grassWave"), mGrassWave);
    glBindVertexArray(mVaoPlayer);
    glDrawElementsInstanced(GL_TRIANGLES, mVertices2.size(), GL_UNSIGNED_INT, 0, (GLsizei)mOffsets.size());
    refreshShaderTransforms(shaderProgramID);
}

void Player::refreshShaderTransforms(int shaderProgramID) {
    // revert too unity as model matrix, ie no transforms
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1)));
}

void Player::initGL() {
    // test fill the offsetarray (contains offset vectors)
    // vertexBuffer
    glGenBuffers(1, &mVbo);
    glGenBuffers(1, &mInstanceVBO);
    glGenBuffers(1, &mTintVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, mOffsets.size() * sizeof(glm::vec3), &mOffsets[0], GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2) * mVertices2.size(), &mVertices2[0], GL_STATIC_DRAW);

    glGenVertexArrays(1, &mVaoPlayer);
    glBindVertexArray(mVaoPlayer);
    // create bind and upload

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // location
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), 0);
    // color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)sizeof(glm::vec3));
    // Normals
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(2 * sizeof(glm::vec3)));
    // Materials
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (GLvoid *)(3 * sizeof(glm::vec3)));

    // instancing: per-instance world offset (location 4)
    glEnableVertexAttribArray(4);
    glBindBuffer(GL_ARRAY_BUFFER, mInstanceVBO);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glVertexAttribDivisor(4, 1);

    // per-instance colour tint + emissive strength (location 5)
    glBindBuffer(GL_ARRAY_BUFFER, mTintVBO);
    glBufferData(GL_ARRAY_BUFFER, mTintEmissive.size() * sizeof(glm::vec4), &mTintEmissive[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(5, 1);

    mOffsetsChanged = false;

    ////indices
    glGenBuffers(1, &mIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*mIndices.size(), &mIndices[0], GL_STATIC_DRAW);
    std::cout << "drawing " << mVertices2.size() << " vertices with " << mIndices.size() << " indices" << std::endl;
    std::cout << "initialised  player gl" << std::endl;
}

void Player::loadDefaultGeometry() {
    this->loadGeometry(mGeomPath);
}
void Player::cleanup() {
    glDeleteVertexArrays(1, &mVaoPlayer);
    glDeleteBuffers(1, &mVbo);
    glDeleteBuffers(1, &mInstanceVBO);
    glDeleteBuffers(1, &mTintVBO);
    glDeleteBuffers(1, &mIbo);
}
