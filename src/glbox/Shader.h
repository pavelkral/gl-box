#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

class Shader
{
public:
    // void setBool(const std::string &name, bool value);
    // void setInt(const std::string &name, int value);
    // void setFloat(const std::string &name, float value);
    // void setVec2(const std::string &name, const glm::vec2 &value);
    // void setVec2(const std::string &name, float x, float y);
    // void setVec3(const std::string &name, const glm::vec3 &value);
    // void setVec3(const std::string &name, float x, float y, float z);
    // void setVec4(const std::string &name, const glm::vec4 &value);
    // void setVec4(const std::string &name, float x, float y, float z, float w);
    // void setMat2(const std::string &name, const glm::mat2 &mat);
    // void setMat3(const std::string &name, const glm::mat3 &mat);
    // void setMat4(const std::string &name, const glm::mat4 &mat);
    unsigned int ID;

    Shader(const char *vertexPath, const char *fragmentPath) {
        createProgram(vertexPath, fragmentPath);
    }
    ~Shader(){

    }
    void createProgram(const char *vertexPath,
                       const char *fragmentPath){
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            vShaderFile.close();
            fShaderFile.close();
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure &e) {
            std::cout << "CHYBA::SHADER::SOUBOR_NEBYL_USPESNE_PRECTEN" << std::endl;
        }
        const char *vShaderCode = vertexCode.c_str();
        const char *fShaderCode = fragmentCode.c_str();
        unsigned int vertex, fragment;
        int success;
        char infoLog[512];
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cout << "CHYBA::SHADER::VERTEX::KOMPILACE_NEUSPESNA\n"
                      << infoLog << std::endl;
        };
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cout << "CHYBA::SHADER::FRAGMENT::KOMPILACE_NEUSPESNA\n"
                      << infoLog << std::endl;
        };
        unsigned int program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, NULL, infoLog);
            std::cout << "CHYBA::PROGRAM::LINKOVANI_NEUSPESNE\n"
                      << infoLog << std::endl;
        }
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        //return program;
        ID = program;
    }



private:
};

#endif // SHADER_H
