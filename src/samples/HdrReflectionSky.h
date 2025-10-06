#ifndef HDRREFLECTIONSKY_H
#define HDRREFLECTIONSKY_H
// main.cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
// #include "stb_image.h" // Již není nutné, je v HdriSky.h

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>

// --- Inkluze nově vytvořených hlavičkových souborů ---
#include "../glbox/HdriSky.h"
#include "../glbox/geometry/Sphere.h"

// --- Konstanty, Kamera, Časování (nemění se) ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  SCR_WIDTH / 2.0f;
float lastY =  SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Prototypy (pouze callbacky a pomocné funkce) ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);


int main()
{
    // ... (GLFW a GLAD inicializace, callbacky, Depth Test) ...
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"HDRI Skybox",NULL,NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cout<<"GLAD failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // ------------------------------------------------------------------
    // *** Vytvoření objektů a inicializace ***
    // ------------------------------------------------------------------

    HdriSky sky;
    Sphere sphereLeft;
    Sphere sphereRight;
    Sphere sphereCenter;

    // Inicializace objektů
    sky.init("assets/sky.hdr");
    sphereLeft.init();
    sphereRight.init();
    sphereCenter.init();


    // Hlavní smyčka
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH/(float)SCR_HEIGHT,0.1f,100.0f);
        glm::mat4 view = glm::lookAt(cameraPos,cameraPos+cameraFront,cameraUp);

        // --- 1. Vykreslení Skyboxu (HdriSky) ---
        // Skybox musíme vykreslit jako první nebo po všech neprůhledných objektech.
        // Pro zjednodušení jej vykreslíme zde.
        sky.draw(view, projection);

        // --- 1. Koule A: STŘÍBRNÝ CHROM (Vlevo) ---
        glm::mat4 modelA = glm::mat4(1.0f);
        modelA = glm::translate(modelA, glm::vec3(-3.5f, 0.0f, 0.0f));
        modelA = glm::scale(modelA, glm::vec3(1.2f));
        sphereLeft.setMaterial(0, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 1.0f, 5.0f, 1.0f);
        sphereLeft.draw(sky.getCubemapTexture(), modelA, view, projection, cameraPos, false);

        // --- 2. Koule B: ČIRÉ SKLO (Uprostřed) - UPRAVENO ---
        glm::mat4 modelB = glm::mat4(1.0f);
        modelB = glm::translate(modelB, glm::vec3(0.0f, 0.0f, 0.0f));
        modelB = glm::scale(modelB, glm::vec3(1.5f));

        // Mode 0 (Reflexe/Sklo), Barva Bílá (Čiré), Alpha 0.2f (Transparentní)
        // IOR 1/1.52, Fresnel Power 5.0f, Reflection Strength 0.1f (Nízký)
        const float IOR_GLASS = 1.0f / 1.52f; // Index lomu pro sklo
        sphereCenter.setMaterial(0, glm::vec3(1.0f, 1.0f, 1.0f), 0.2f, IOR_GLASS, 5.0f, 0.1f);
        sphereCenter.draw(sky.getCubemapTexture(), modelB, view, projection, cameraPos, true); // DŮLEŽITÉ: true pro transparentnost

        // --- 3. Koule C: Tmavě MODRÝ CHROM (Vpravo) ---
        glm::mat4 modelC = glm::mat4(1.0f);
        modelC = glm::translate(modelC, glm::vec3(3.5f, 0.0f, 0.0f));
        modelC = glm::scale(modelC, glm::vec3(1.2f));

        sphereRight.setMaterial(0, glm::vec3(0.2f, 0.2f, 1.0f), 1.0f, 1.0f, 5.0f, 1.0f);
        sphereRight.draw(sky.getCubemapTexture(), modelC, view, projection, cameraPos, false);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


// --- Pomocné funkce (Callbacky) ---

void framebuffer_size_callback(GLFWwindow* window,int width,int height){ glViewport(0,0,width,height); }
void processInput(GLFWwindow *window)
{
    float cameraSpeed = 2.5f * deltaTime;
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if(firstMouse)
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

    yaw   += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f) pitch = 89.0f;
    if(pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}


#endif // HDRREFLECTIONSKY_H
