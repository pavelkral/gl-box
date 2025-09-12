#ifndef MATERIAL_H
#define MATERIAL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Material {
public:
    unsigned int shaderProgramID;
    std::vector<Texture> textures;
    unsigned int shadowMapID;

    Material(const char* vertexPath, const char* fragmentPath, const std::vector<Texture>& textures, unsigned int sshadowMap) {
        // Shader loading and compilation code
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
            std::cout << "CHYBA::SHADER::SOUBOR_NEBYL_USPESNE_PRECTEN: " << e.what() << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        shaderProgramID = glCreateProgram();
        glAttachShader(shaderProgramID, vertex);
        glAttachShader(shaderProgramID, fragment);
        glLinkProgram(shaderProgramID);
        checkCompileErrors(shaderProgramID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        // Store textures
        this->textures = textures;
        this->shadowMapID = sshadowMap;

    }

    void use(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {

        if (this->shaderProgramID == 0) {
            std::cout << "CHYBA: Shader Program ID je 0. Shader se nepodarilo nacist." << std::endl;
        }
        glUseProgram(this->shaderProgramID);
        setMat4("model", model);
        setMat4("view", view);
        setMat4("projection", projection);
        setMat4("lightSpaceMatrix", lightSpaceMatrix);

        int loc = glGetUniformLocation(this->shaderProgramID, "shadowMap");
        if (loc == -1) {
            std::cout << "CHYBA: Uniform 'shadowMap' nebyl nalezen v shaderu!" << std::endl;
        } else {
            glUniform1i(loc, textures.size());
        }
        // texture binding
        unsigned int diffuseNr = 1;
        unsigned int normalNr = 1;

        for (unsigned int i = 0; i < this->textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            std::string number;
            std::string name = this->textures[i].type;

            if (name == "texture_diffuse") number = std::to_string(diffuseNr++);
            else if (name == "texture_normal") number = std::to_string(normalNr++);

            glUniform1i(glGetUniformLocation(this->shaderProgramID, (name + number).c_str()), i);
            glBindTexture(GL_TEXTURE_2D, this->textures[i].id);
        }
        // ** SHADOW MAP**
        glActiveTexture(GL_TEXTURE0 + textures.size());
        glBindTexture(GL_TEXTURE_2D, this->shadowMapID);
        glUniform1i(glGetUniformLocation(this->shaderProgramID, "shadowMap"), textures.size());
    }

    void setLightProperties(const glm::vec3& light_pos, const glm::vec3& light_color, float ambient_strength, const glm::vec3& view_pos) const {
        glUseProgram(this->shaderProgramID);
        setVec3("lightPos", light_pos);
        setVec3("lightColor", light_color);
        setFloat("ambientStrength", ambient_strength);
        setVec3("viewPos", view_pos);
    }

private:
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(this->shaderProgramID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void setVec3(const std::string& name, const glm::vec3& vec) const {
        glUniform3fv(glGetUniformLocation(this->shaderProgramID, name.c_str()), 1, glm::value_ptr(vec));
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(this->shaderProgramID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(this->shaderProgramID, name.c_str()), value);
    }
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "CHYBA::SHADER_KOMPILACE_CHYBA::" << type << "\n" << infoLog << std::endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "CHYBA::PROGRAM_LINKOVANI_CHYBA::" << type << "\n" << infoLog << std::endl;
            }
        }
    }
};

#endif
