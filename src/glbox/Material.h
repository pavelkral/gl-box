#ifndef MATERIAL_H
#define MATERIAL_H



#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Material {
public:
    unsigned int shaderProgramID;
    unsigned int diffuseTextureID;
    unsigned int shadowMapID;

    Material(const char* vertexPath, const char* fragmentPath, unsigned int diffuseTex, unsigned int shadowMap) {
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
        } catch (std::ifstream::failure& e) {
            std::cout << "CHYBA::SHADER::SOUBOR_NEBYL_USPESNE_PRECTEN" << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;
        int success;
        char infoLog[512];

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cout << "CHYBA::SHADER::VERTEX::KOMPILACE_NEUSPESNA\n" << infoLog << std::endl;
        };

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cout << "CHYBA::SHADER::FRAGMENT::KOMPILACE_NEUSPESNA\n" << infoLog << std::endl;
        };

        shaderProgramID = glCreateProgram();
        glAttachShader(shaderProgramID, vertex);
        glAttachShader(shaderProgramID, fragment);
        glLinkProgram(shaderProgramID);
        glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgramID, 512, NULL, infoLog);
            std::cout << "CHYBA::PROGRAM::LINKOVANI_NEUSPESNE\n" << infoLog << std::endl;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        diffuseTextureID = diffuseTex;
        shadowMapID = shadowMap;
    }

    void use(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        glUseProgram(shaderProgramID);
        setMat4("model", model);
        setMat4("view", view);
        setMat4("projection", projection);
        setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseTextureID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowMapID);
        setInt("diffuseTexture", 0);
        setInt("shadowMap", 1);
    }
    void setLightProperties(const glm::vec3& light_pos, const glm::vec3& light_color, float ambient_strength, const glm::vec3& view_pos) const {
        glUseProgram(shaderProgramID);
        setVec3("lightPos", light_pos);
        setVec3("lightColor", light_color);
        setFloat("ambientStrength", ambient_strength);
        setVec3("viewPos", view_pos);
    }
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void setVec3(const std::string& name, const glm::vec3& vec) const {
        glUniform3fv(glGetUniformLocation(shaderProgramID, name.c_str()), 1, glm::value_ptr(vec));
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(shaderProgramID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(shaderProgramID, name.c_str()), value);
    }

};
#endif // MATERIAL_H
