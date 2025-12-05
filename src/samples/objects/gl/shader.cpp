#include "shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath) {
    std::string vCode = loadFile(vertexPath);
    std::string fCode = loadFile(fragmentPath);

    const char* vSrc = vCode.c_str();
    const char* fSrc = fCode.c_str();

    GLuint vs = compileShader(GL_VERTEX_SHADER, vSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fSrc);

    ID = glCreateProgram();
    glAttachShader(ID, vs);
    glAttachShader(ID, fs);
    glLinkProgram(ID);
    checkLinkErrors(ID);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (ID) glDeleteProgram(ID);
}

Shader::Shader(Shader &&other) noexcept : ID(other.ID) {
    other.ID = 0;
}

void Shader::setInt(const std::string &name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string &name, const glm::vec3 &vec) const {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec));
}

void Shader::setVec4(const std::string &name, const glm::vec4 &vec) const {
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec));
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

std::string Shader::loadFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Unable to open shader file " << path << std::endl;
        return "";
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Remove BOMs
    while (!content.empty()) {
        unsigned char c = (unsigned char)content[0];

        // UTF-8 BOM EF BB BF
        if (content.size() >= 3 && c == 0xEF &&
            (unsigned char)content[1] == 0xBB &&
            (unsigned char)content[2] == 0xBF)
        {
            content.erase(0, 3);
            continue;
        }

        // FEFF BOM
        if (c == 0xFE || c == 0xFF) {
            content.erase(0, 1);
            continue;
        }

        // NO-BREAK SPACE C2 A0
        if (c == 0xC2 && content.size() >= 2 &&
            (unsigned char)content[1] == 0xA0)
        {
            content.erase(0, 2);
            continue;
        }

        break;
    }

    return content;
}

GLuint Shader::compileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    checkCompileErrors(shader, type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
    return shader;
}

void Shader::checkCompileErrors(GLuint shader, const std::string &type) {
    GLint success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "SHADER_ERROR::" << type << "\n" << infoLog << std::endl;
    }
}

void Shader::checkLinkErrors(GLuint program) {
    GLint success;
    char infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "PROGRAM_LINK_ERROR\n" << infoLog << std::endl;
    }
}
