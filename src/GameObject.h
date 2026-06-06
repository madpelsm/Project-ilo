#pragma once

#include "Vertex.h"
#include "GL.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
class GameObject {
  public:
    virtual ~GameObject() = default;
    virtual void render(int shaderProgram) = 0;
};