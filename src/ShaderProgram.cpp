#include "ShaderProgram.h"

ShaderProgram::ShaderProgram() {
}
ShaderProgram::~ShaderProgram() {
    deleteProgram();
}
bool ShaderProgram::linkProgram() {
    GLint program = programID;
    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

        // The program is useless now. So delete it.
        glDeleteProgram(program);

        // Provide the infolog in whatever manner you deem best.
        std::cout << "GLSL ShaderProgram error:" << std::endl;
        for (unsigned int i = 0; i < infoLog.size(); i++) {
            std::cout << infoLog[i];
        }
        // Exit with failure.
        return 0;
    }
    bLinked = true;
    return 1;
}
void ShaderProgram::useProgram() {
    glUseProgram(programID);
}
void ShaderProgram::createProgram() {
    programID = glCreateProgram();
}
bool ShaderProgram::attachShaderToProgram(Shader *shader) {
    if (!shader->isLoaded()) {
        return false;
    }
    glAttachShader(programID, shader->getShaderID());
    return true;
}
int ShaderProgram::getProgramID() {
    return programID;
}

void ShaderProgram::deleteProgram() {
    if (programID)
        glDeleteProgram(programID);
    programID = 0;
    bLinked = false;
}
