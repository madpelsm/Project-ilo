#pragma once
#include <fstream>
#include <glad/glad.h>
#include <stdio.h>
#include <vector>
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <iostream>
class Shader {
  public:
    Shader();
    bool loadShader(std::string pos, int shaderType);
    void deleteShader();
    int getShaderID();
    bool isLoaded();

  private:
    unsigned int shaderID = 0;
    int shaderType = 0;
    bool loaded = false;
};