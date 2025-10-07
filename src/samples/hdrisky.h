#ifndef HDRISKY_H
#define HDRISKY_H
//#include "samples/deferred.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include <iostream>
#include <vector>
#include <string>

// --- Prototypy funkcí ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);
void renderCube();

// --- Nastavení ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Kamera ---
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

// --- Ovládání myší ---
bool firstMouse = true;
float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  SCR_WIDTH / 2.0;
float lastY =  SCR_HEIGHT / 2.0;

// --- Časování ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Shaders ---

// 1. Shader pro konverzi Equirectangular mapy na Cubemapu
const char *equirectToCubemapVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 WorldPos;
uniform mat4 projection;
uniform mat4 view;
void main()
{
    WorldPos = aPos;
    gl_Position =  projection * view * vec4(aPos, 1.0);
}
)glsl";

const char *equirectToCubemapFragmentShaderSource = R"glsl(
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

// 2. Shader pro vykreslení finálního skyboxu (s tonemappingem)
const char *skyboxVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 projection;
uniform mat4 view;
out vec3 WorldPos;
void main()
{
    WorldPos = aPos;
    gl_Position = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
}
)glsl";

const char *skyboxFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 WorldPos;
uniform samplerCube environmentMap;
void main()
{
    vec3 envColor = texture(environmentMap, WorldPos).rgb;
    // Tonemapping (Reinhard) a Gamma korekce
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2));
    FragColor = vec4(envColor, 1.0);
}
)glsl";


// 3. Shader pro chromovou krychli (vertex stejný, fragment s tonemappingem)
const char *cubeVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
out vec3 Normal;
out vec3 Position;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    Normal = mat3(transpose(inverse(model))) * aNormal;
    Position = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

const char *cubeFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 Position;
uniform vec3 cameraPos;
uniform samplerCube environmentMap;
void main()
{
    vec3 I = normalize(Position - cameraPos);
    vec3 R = reflect(I, normalize(Normal));
    vec3 envColor = texture(environmentMap, R).rgb;
    // Tonemapping (Reinhard) a Gamma korekce
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2));
    FragColor = vec4(envColor, 1.0);
}
)glsl";

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;

int main()
{
    // --- Inicializace ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "HDRI Skybox", NULL, NULL);
    if (window == NULL) { std::cout << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to initialize GLAD" << std::endl; return -1; }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // --- Kompilace shaderů ---
    // Program 1: Konverze
    unsigned int V1 = glCreateShader(GL_VERTEX_SHADER); glShaderSource(V1, 1, &equirectToCubemapVertexShaderSource, NULL); glCompileShader(V1);
    unsigned int F1 = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(F1, 1, &equirectToCubemapFragmentShaderSource, NULL); glCompileShader(F1);
    unsigned int equirectToCubemapShader = glCreateProgram(); glAttachShader(equirectToCubemapShader, V1); glAttachShader(equirectToCubemapShader, F1); glLinkProgram(equirectToCubemapShader);
    glDeleteShader(V1); glDeleteShader(F1);

    // Program 2: Skybox
    unsigned int V2 = glCreateShader(GL_VERTEX_SHADER); glShaderSource(V2, 1, &skyboxVertexShaderSource, NULL); glCompileShader(V2);
    unsigned int F2 = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(F2, 1, &skyboxFragmentShaderSource, NULL); glCompileShader(F2);
    unsigned int skyboxShader = glCreateProgram(); glAttachShader(skyboxShader, V2); glAttachShader(skyboxShader, F2); glLinkProgram(skyboxShader);
    glDeleteShader(V2); glDeleteShader(F2);

    // Program 3: Krychle
    unsigned int V3 = glCreateShader(GL_VERTEX_SHADER); glShaderSource(V3, 1, &cubeVertexShaderSource, NULL); glCompileShader(V3);
    unsigned int F3 = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(F3, 1, &cubeFragmentShaderSource, NULL); glCompileShader(F3);
    unsigned int cubeShader = glCreateProgram(); glAttachShader(cubeShader, V3); glAttachShader(cubeShader, F3); glLinkProgram(cubeShader);
    glDeleteShader(V3); glDeleteShader(F3);

    glUseProgram(equirectToCubemapShader);
    glUniform1i(glGetUniformLocation(equirectToCubemapShader, "equirectangularMap"), 0);
    glUseProgram(skyboxShader);
    glUniform1i(glGetUniformLocation(skyboxShader, "environmentMap"), 0);
    glUseProgram(cubeShader);
    glUniform1i(glGetUniformLocation(cubeShader, "environmentMap"), 0);


    // --- KROK 1: NAČTENÍ HDRI TEXTURY ---
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf("assets/texture/sky.hdr", &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image." << std::endl;
        return -1;
    }

    // --- KROK 2: KONVERZE HDRI NA CUBEMAPU ---
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    unsigned int envCubemap;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

    glUseProgram(equirectToCubemapShader);
    glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- KROK 3: VYKRESLENÍ SCÉNY ---
    int scrWidth, scrHeight;
    glfwGetFramebufferSize(window, &scrWidth, &scrHeight);
    glViewport(0, 0, scrWidth, scrHeight);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Vykreslení chromové krychle
        glUseProgram(cubeShader);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * glm::radians(25.0f), glm::vec3(0.5f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(cubeShader, "cameraPos"), 1, glm::value_ptr(cameraPos));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        renderCube();

        // Vykreslení skyboxu
        glUseProgram(skyboxShader);
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        renderCube();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
    glfwTerminate();
    return 0;
}

void renderCube()
{
    if (cubeVAO == 0)
    {
        float vertices[] = {
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
            1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

#endif // HDRISKY_H
