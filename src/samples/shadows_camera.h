#ifndef SHADOWS_CAMERA_H
#define SHADOWS_CAMERA_H
#define SD_ENABLE_IRRKLANG 1

#include <iostream>
#include <chrono>
#include <thread>
#include "../lib/glad/include/glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "glfw/glfw3.h"
#include "../lib/ThreadPool.h"
#include "../lib/stb/image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>

// Přejmenujte Camera.h podle názvu souboru s vaší kamerou
#include "../glbox/Camera.h"

// Prototypy funkcí
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
void checkCompileErrors(unsigned int shader, std::string type);

// Nastavení
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Globální instance kamery
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Shaders (beze změn)
// ... (Zde je váš kód shaderů)
const char *sceneVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;

    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoords;
    out vec4 FragPosLightSpace;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform mat4 lightSpaceMatrix;

    void main()
    {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
        gl_Position = projection * view * model * vec4(aPos, 1.0); // Změna zde: aPos namísto FragPos pro správnou projekci
    }
)";

const char *sceneFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoords;
    in vec4 FragPosLightSpace;

    uniform sampler2D diffuseTexture;
    uniform sampler2DShadow shadowMap;

    uniform vec3 lightPos;
    uniform vec3 viewPos;

    float calculateShadow(vec4 fragPosLightSpace)
    {
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;

        if(projCoords.z > 1.0)
            return 1.0;

        float currentDepth = projCoords.z;
        float bias = 0.005;

        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth - bias));
                shadow += pcfDepth;
            }
        }
        shadow /= 9.0;

        return shadow;
    }

    void main()
    {
        vec3 color = texture(diffuseTexture, TexCoords).rgb;
        vec3 normal = normalize(Normal);
        vec3 lightColor = vec3(1.0);

        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * lightColor;

        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        float shadow = calculateShadow(FragPosLightSpace);
        vec3 lighting = (ambient + shadow * diffuse) * color;

        FragColor = vec4(lighting, 1.0);
    }
)";

const char *depthVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 lightSpaceMatrix;
    uniform mat4 model;
    void main()
    {
        gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
    }
)";
const char *depthFragmentShaderSource = R"(
    #version 330 core
    void main() {}
)";

int main()
{
    // ... (Inicializace GLFW a GLAD - beze změn)
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Shadow Mapping", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // ... (Kompilace shaderů, VBO/VAO, načítání textur - beze změn)
    unsigned int sceneVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(sceneVS, 1, &sceneVertexShaderSource, NULL);
    glCompileShader(sceneVS);
    checkCompileErrors(sceneVS, "VERTEX");

    unsigned int sceneFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sceneFS, 1, &sceneFragmentShaderSource, NULL);
    glCompileShader(sceneFS);
    checkCompileErrors(sceneFS, "FRAGMENT");

    unsigned int sceneShader = glCreateProgram();
    glAttachShader(sceneShader, sceneVS);
    glAttachShader(sceneShader, sceneFS);
    glLinkProgram(sceneShader);
    checkCompileErrors(sceneShader, "PROGRAM");
    glDeleteShader(sceneVS);
    glDeleteShader(sceneFS);

    unsigned int depthVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(depthVS, 1, &depthVertexShaderSource, NULL);
    glCompileShader(depthVS);
    checkCompileErrors(depthVS, "VERTEX");

    unsigned int depthFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(depthFS, 1, &depthFragmentShaderSource, NULL);
    glCompileShader(depthFS);
    checkCompileErrors(depthFS, "FRAGMENT");

    unsigned int depthShader = glCreateProgram();
    glAttachShader(depthShader, depthVS);
    glAttachShader(depthShader, depthFS);
    glLinkProgram(depthShader);
    checkCompileErrors(depthShader, "PROGRAM");
    glDeleteShader(depthVS);
    glDeleteShader(depthFS);

    float cubeVertices[] = { -0.5f,-0.5f,-0.5f,0.0f,0.0f,-1.0f,0.0f,0.0f,0.5f,-0.5f,-0.5f,0.0f,0.0f,-1.0f,1.0f,0.0f,0.5f,0.5f,-0.5f,0.0f,0.0f,-1.0f,1.0f,1.0f,0.5f,0.5f,-0.5f,0.0f,0.0f,-1.0f,1.0f,1.0f,-0.5f,0.5f,-0.5f,0.0f,0.0f,-1.0f,0.0f,1.0f,-0.5f,-0.5f,-0.5f,0.0f,0.0f,-1.0f,0.0f,0.0f,-0.5f,-0.5f,0.5f,0.0f,0.0f,1.0f,0.0f,0.0f,0.5f,-0.5f,0.5f,0.0f,0.0f,1.0f,1.0f,0.0f,0.5f,0.5f,0.5f,0.0f,0.0f,1.0f,1.0f,1.0f,0.5f,0.5f,0.5f,0.0f,0.0f,1.0f,1.0f,1.0f,-0.5f,0.5f,0.5f,0.0f,0.0f,1.0f,0.0f,1.0f,-0.5f,-0.5f,0.5f,0.0f,0.0f,1.0f,0.0f,0.0f,-0.5f,0.5f,0.5f,-1.0f,0.0f,0.0f,1.0f,0.0f,-0.5f,0.5f,-0.5f,-1.0f,0.0f,0.0f,1.0f,1.0f,-0.5f,-0.5f,-0.5f,-1.0f,0.0f,0.0f,0.0f,1.0f,-0.5f,-0.5f,-0.5f,-1.0f,0.0f,0.0f,0.0f,1.0f,-0.5f,-0.5f,0.5f,-1.0f,0.0f,0.0f,0.0f,0.0f,-0.5f,0.5f,0.5f,-1.0f,0.0f,0.0f,1.0f,0.0f,0.5f,0.5f,0.5f,1.0f,0.0f,0.0f,1.0f,0.0f,0.5f,0.5f,-0.5f,1.0f,0.0f,0.0f,1.0f,1.0f,0.5f,-0.5f,-0.5f,1.0f,0.0f,0.0f,0.0f,1.0f,0.5f,-0.5f,-0.5f,1.0f,0.0f,0.0f,0.0f,1.0f,0.5f,-0.5f,0.5f,1.0f,0.0f,0.0f,0.0f,0.0f,0.5f,0.5f,0.5f,1.0f,0.0f,0.0f,1.0f,0.0f,-0.5f,-0.5f,-0.5f,0.0f,-1.0f,0.0f,0.0f,1.0f,0.5f,-0.5f,-0.5f,0.0f,-1.0f,0.0f,1.0f,1.0f,0.5f,-0.5f,0.5f,0.0f,-1.0f,0.0f,1.0f,0.0f,0.5f,-0.5f,0.5f,0.0f,-1.0f,0.0f,1.0f,0.0f,-0.5f,-0.5f,0.5f,0.0f,-1.0f,0.0f,0.0f,0.0f,-0.5f,-0.5f,-0.5f,0.0f,-1.0f,0.0f,0.0f,1.0f,-0.5f,0.5f,-0.5f,0.0f,1.0f,0.0f,0.0f,1.0f,0.5f,0.5f,-0.5f,0.0f,1.0f,0.0f,1.0f,1.0f,0.5f,0.5f,0.5f,0.0f,1.0f,0.0f,1.0f,0.0f,0.5f,0.5f,0.5f,0.0f,1.0f,0.0f,1.0f,0.0f,-0.5f,0.5f,0.5f,0.0f,1.0f,0.0f,0.0f,0.0f,-0.5f,0.5f,-0.5f,0.0f,1.0f,0.0f,0.0f,1.0f };
    float planeVertices[] = { 25.0f,-0.5f,25.0f,0.0f,1.0f,0.0f,10.0f,0.0f,-25.0f,-0.5f,25.0f,0.0f,1.0f,0.0f,0.0f,0.0f,-25.0f,-0.5f,-25.0f,0.0f,1.0f,0.0f,0.0f,10.0f,25.0f,-0.5f,25.0f,0.0f,1.0f,0.0f,10.0f,0.0f,-25.0f,-0.5f,-25.0f,0.0f,1.0f,0.0f,0.0f,10.0f,25.0f,-0.5f,-25.0f,0.0f,1.0f,0.0f,10.0f,10.0f };

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    unsigned int cubeTexture = loadTexture("floor.png");
    unsigned int floorTexture = loadTexture("floor.png");

    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(sceneShader);
    glUniform1i(glGetUniformLocation(sceneShader, "diffuseTexture"), 0);
    glUniform1i(glGetUniformLocation(sceneShader, "shadowMap"), 1);

    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

    while (!glfwWindowShouldClose(window))
    {
        // Vypočet delta času
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Zpracování vstupu
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Renderování do depth mapy
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane = 1.0f, far_plane = 7.5f;
        lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;

        glUseProgram(depthShader);
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));
        model = glm::scale(model, glm::vec3(0.5f));
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Reset viewportu
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(sceneShader);

        // Aktualizace matic pro zobrazení
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniform3fv(glGetUniformLocation(sceneShader, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3fv(glGetUniformLocation(sceneShader, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));
        model = glm::scale(model, glm::vec3(0.5f));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &planeVBO);
    glfwTerminate();
    return 0;
}

// Implementace pomocných funkcí
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos; // Změna osy Y
    lastX = (float)xpos;
    lastY = (float)ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll((float)yoffset);
}

// Funkce loadTexture a checkCompileErrors jsou beze změn
unsigned int loadTexture(char const * path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 4) format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

void checkCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "CHYBA::SHADER_KOMPILACE typu: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "CHYBA::PROGRAM_LINKOVANI typu: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

#endif // SHADOWS_CAMERA_H
