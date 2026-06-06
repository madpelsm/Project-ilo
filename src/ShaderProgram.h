#pragma once
#include "Shader.h"
#include "GL.h"
class ShaderProgram {
  public:
    GLuint programID = 0;
    bool bLinked = false;

    ShaderProgram();
    ~ShaderProgram();
    bool linkProgram();
    void useProgram();
    void createProgram();
    bool attachShaderToProgram(Shader *shader);
    int getProgramID();
    void deleteProgram();
};