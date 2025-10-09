#ifndef TEXTUREDSKY_H
#define TEXTUREDSKY_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <stb_image.h>


unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int texID; glGenTextures(1,&texID); glBindTexture(GL_TEXTURE_CUBE_MAP,texID);
    int w,h,n;
    stbi_set_flip_vertically_on_load(false);
    for(unsigned int i=0;i<faces.size();i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(),&w,&h,&n,0);
        if(data){
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,data);
            stbi_image_free(data);
        } else { std::cout<<"Cubemap failed: "<<faces[i]<<"\n"; }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    return texID;
}

unsigned int loadTexture(const char* path)
{
    unsigned int tex; glGenTextures(1,&tex);
    int w,h,n; unsigned char* data = stbi_load(path,&w,&h,&n,0);
    if(data){
        GLenum format = n==4?GL_RGBA:GL_RGB;
        glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,format,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    } else { std::cout<<"Texture failed: "<<path<<"\n"; }
    stbi_image_free(data);
    return tex;
}


class TexturedSky {
public:

    TexturedSky(const std::vector<std::string>& faces) {

        InitShaders();
        InitData();
        cubemapTexture = loadCubemap(faces);

        glUseProgram(shaderProgram);
        glUniform1i(glGetUniformLocation(shaderProgram, "skybox"), 0);
    }

    void Draw(const glm::mat4& view, const glm::mat4& projection) {
        // Nastavení pro kreslení skyboxu
        glDepthFunc(GL_LEQUAL); // Změníme funkci hloubkového testu

        glUseProgram(shaderProgram);

        // Z view matice odstraníme složky translace, aby se skybox pohyboval s kamerou
        // ale nepromítal se do prostoru (zůstane "nekonečně" daleko).
        glm::mat4 skyView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyView));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        // Vrátíme funkci hloubkového testu na původní hodnotu
        glDepthFunc(GL_LESS);
    }


    ~TexturedSky() {
        glDeleteVertexArrays(1, &skyboxVAO);
        glDeleteBuffers(1, &skyboxVBO);
        glDeleteProgram(shaderProgram);
        glDeleteTextures(1, &cubemapTexture);
    }

private:

    unsigned int skyboxVAO, skyboxVBO, shaderProgram, cubemapTexture;

    // TexturedSky vertex shader
    const char *skyboxVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;
    uniform mat4 projection;
    uniform mat4 view;
    void main()
    {
        TexCoords = aPos;
        // Odstranění translace z view matice pro nekonečně vzdálený skybox
        vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
        gl_Position = pos.xyww; // Použití z ve w pro zaručení maximální hloubky
    }
    )";


        const char *skyboxFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec3 TexCoords;
    uniform samplerCube skybox;
    void main()
    {
        FragColor = texture(skybox, TexCoords);
    }
    )";


    unsigned int compileShader(const char* src, GLenum type) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);

        int success;
        char infoLog[512];
        glGetShaderiv(s, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(s, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED of type " << type << "\n" << infoLog << std::endl;
        }
        return s;
    }


    void InitShaders() {
        unsigned int skyVS = compileShader(skyboxVertexShaderSource, GL_VERTEX_SHADER);
        unsigned int skyFS = compileShader(skyboxFragmentShaderSource, GL_FRAGMENT_SHADER);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, skyVS);
        glAttachShader(shaderProgram, skyFS);
        glLinkProgram(shaderProgram);

        int success;
        char infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        glDeleteShader(skyVS);
        glDeleteShader(skyFS);
    }

    void InitData() {
        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &skyboxVAO);
        glGenBuffers(1, &skyboxVBO);
        glBindVertexArray(skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0); // Odvázání VAO
    }
};

#endif // TEXTUREDSKY_H
