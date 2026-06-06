#include "Shader.h"
#include <iostream>

Shader::Shader() {
}
bool Shader::loadShader(std::string pos, int shaderType) {
    loaded = false;
    std::ifstream t(pos);
    if (!t.is_open()) {
        std::cout << "Shader file not found: " << pos << std::endl;
        return false;
    }
    std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
#ifdef __EMSCRIPTEN__
    // Translate the desktop GLSL 330 core shaders to WebGL2 GLSL ES 300: swap the
    // first line (#version ...) for the ES version + default precision qualifiers.
    {
        size_t nl = str.find('\n');
        std::string body = (nl == std::string::npos) ? std::string() : str.substr(nl + 1);
        str = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n" + body;
    }
#endif
    shaderID = glCreateShader(shaderType);
    const char *c_str = str.c_str();
    if (shaderID == 0) {
        std::cout << "error when creating shader" << std::endl;
    }
    glShaderSource(shaderID, 1, &c_str, nullptr);
    glCompileShader(shaderID);

    // debugging
    GLint isCompiled = 0;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shaderID, maxLength, &maxLength, &errorLog[0]);

        // Provide the infolog in whatever manor you deem best.
        std::cout << "GLSL Shader error:" << std::endl;
        for (unsigned int i = 0; i < errorLog.size(); i++) {
            std::cout << errorLog[i];
        }
        // Exit with failure.
        glDeleteShader(shaderID); // Don't leak the shader.
        return false;
    }
    this->shaderType = shaderType;
    loaded = true;
    return 1;
}

int Shader::getShaderID() {
    return shaderID;
}
bool Shader::isLoaded() {
    return loaded;
}

void Shader::deleteShader() {
    if (!loaded)
        return;
    loaded = false;
    glDeleteShader(shaderID);
}
