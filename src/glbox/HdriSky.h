// HdriSky.h
#ifndef HDRI_SKY_H
#define HDRI_SKY_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include "stb_image.h"
#include "Shader.h"

const char* equirectToCubemapVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 WorldPos;
uniform mat4 projection;
uniform mat4 view;
void main()
{
    WorldPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
)glsl";

const char* equirectToCubemapFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 WorldPos;
uniform sampler2D equirectangularMap;
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
void main()
{
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    FragColor = vec4(texture(equirectangularMap, uv).rgb, 1.0);
}
)glsl";

const char* skyboxVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    TexCoords = aPos;
    vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)glsl";

const char* skyboxFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube environmentMap;
void main()
{
    vec3 envColor = texture(environmentMap, TexCoords).rgb;
    // Tonemapping a Gamma korekce
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2));
    FragColor = vec4(envColor, 1.0);
}
)glsl";


class HdriSky
{

private:

    unsigned int skyboxShader;
    unsigned int envCubemap;
    unsigned int equirectToCubemapShader;
    static unsigned int cubeVAO, cubeVBO;
    static unsigned int createShader(const char* vs, const char* fs);
    static void renderCube();


public:

    HdriSky() : skyboxShader(0), envCubemap(0), equirectToCubemapShader(0) {}

    void init(const std::string& hdrPath);
    void draw(const glm::mat4& view, const glm::mat4& projection) const;
    unsigned int getCubeMap() const { return envCubemap; }
};


unsigned int HdriSky::cubeVAO = 0;
unsigned int HdriSky::cubeVBO = 0;

inline void HdriSky::renderCube()
{
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // ZÁDA (Z = -10.0f)
            -10.0f,-10.0f,-10.0f,  10.0f,-10.0f,-10.0f,  10.0f, 10.0f,-10.0f,
            10.0f, 10.0f,-10.0f, -10.0f, 10.0f,-10.0f, -10.0f,-10.0f,-10.0f,
            // PŘEDEK (Z = 10.0f)
            -10.0f,-10.0f, 10.0f,  10.0f,-10.0f, 10.0f,  10.0f, 10.0f, 10.0f,
            10.0f, 10.0f, 10.0f, -10.0f, 10.0f, 10.0f, -10.0f,-10.0f, 10.0f,
            // VRCH (Y = 10.0f)
            -10.0f, 10.0f,-10.0f, -10.0f, 10.0f, 10.0f,  10.0f, 10.0f, 10.0f,
            10.0f, 10.0f, 10.0f,  10.0f, 10.0f,-10.0f, -10.0f, 10.0f,-10.0f,
            // SPODEK (Y = -10.0f)
            -10.0f,-10.0f,-10.0f, -10.0f,-10.0f, 10.0f,  10.0f,-10.0f, 10.0f,
            10.0f,-10.0f, 10.0f,  10.0f,-10.0f,-10.0f, -10.0f,-10.0f,-10.0f,
            // LEVÁ STRANA (X = -10.0f)
            -10.0f, 10.0f, 10.0f, -10.0f, 10.0f,-10.0f, -10.0f,-10.0f,-10.0f,
            -10.0f,-10.0f,-10.0f, -10.0f,-10.0f, 10.0f, -10.0f, 10.0f, 10.0f,
            // PRAVÁ STRANA (X = 10.0f)
            10.0f,-10.0f,-10.0f,  10.0f,-10.0f, 10.0f,  10.0f, 10.0f, 10.0f,
            10.0f, 10.0f, 10.0f,  10.0f, 10.0f,-10.0f,  10.0f,-10.0f,-10.0f
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

inline void HdriSky::init(const std::string& hdrPath)
{

    Shader shaderProgram(equirectToCubemapVS, equirectToCubemapFS, true);
    equirectToCubemapShader = shaderProgram.ID;
    Shader shaderProgram1(skyboxVS, skyboxFS, true);
    skyboxShader = shaderProgram1.ID;

    glUseProgram(equirectToCubemapShader);
    glUniform1i(
        glGetUniformLocation(equirectToCubemapShader, "equirectangularMap"), 0);
    glUseProgram(skyboxShader);
    glUniform1i(glGetUniformLocation(skyboxShader, "environmentMap"), 0);
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf(hdrPath.c_str(), &width, &height, &nrComponents, 0);
    if (!data) {
        std::cout << "Failed to load HDRI: " << hdrPath << "\n";
        return;
    }
      unsigned int hdrTexture;
    // Inicializace hdrTexture
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

    // Nastavení pro HDRI (lineární filtrace a žádný wrap)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    // --------------------------------------------------------------------------------------------------
    // (Alokace paměti a nastavení filtru s Mipmaps)
    // --------------------------------------------------------------------------------------------------
    const unsigned int CUBEMAP_RESOLUTION = 512;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    for (unsigned int i = 0; i < 6; i++)
        // Alokace místa pro 0. úroveň (Mipmap 0)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION, 0, GL_RGB, GL_FLOAT, nullptr);

    // Použití Mipmap filtrů pro správné reflexe na dálku
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // ZDE: Mipmaps
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // --------------------------------------------------------------------------------------------------
    //  (Render to Cubemap)
    // --------------------------------------------------------------------------------------------------
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);


    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);


    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 200.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0,0,0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0))
    };

    // Konverze HDRI na Cubemap
    glUseProgram(equirectToCubemapShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION);

    //  vypnout testování hloubky

    glDisable(GL_DEPTH_TEST);

    for(unsigned int i = 0; i < 6; i++)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }


    glEnable(GL_DEPTH_TEST);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- KRITICKÁ KOREKCE: GENERACE MIPMAPS ---
    // Musí být voláno až po renderování do všech 6 stran Cubemapy
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
}
inline void HdriSky::draw(const glm::mat4& view, const glm::mat4& projection) const
{
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyboxShader);

    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view)); // Odstranění translace pro skybox
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    renderCube();
    glDepthFunc(GL_LESS);
}

#endif // HDRI_SKY_H
