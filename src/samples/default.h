#ifndef DEFAULT_H
#define DEFAULT_H
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
#include <string>

#include "glbox/Camera.h"
#include "glbox/Material.h"
#include "glbox/Model.h"
#include "glbox/SceneObject.h"
#include "glbox/StaticMesh.h"
#include "glbox/Transform.h"
#include "glbox/Shader.h"
#include "glbox/Texture.h"
#include "glbox/geometry/CubeMesh.h"
#include "glbox/geometry/PlaneMesh.h"
#include "glbox/Skydome.h"
#include "glbox/Skybox.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
bool cursorEnabled = false;
bool keyLWasPressed = false;
Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main() {

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Gl-box", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    //============================================================================imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    //============================================================================
    // --- DEPTH BUFFER SHADOWS ---

    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH,
                 SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                    GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //============================================================================
    // ---Scene ---

    Shader depthShader("shaders/depth.vert", "shaders/depth.frag");
    Shader modelDepthShader("shaders/model_depth.vert", "shaders/model_depth.frag");

    std::vector<std::string> faces1
        {
            "skybox2/right.bmp",
            "skybox2/left.bmp",
            "skybox2/top.bmp",
            "skybox2/bottom.bmp",
            "skybox2/front.bmp",
            "skybox2/back.bmp"
        };

    unsigned int floorTexID = Loader::Trexture::loadTexture("floor.png");
    unsigned int brickTexID = Loader::Trexture::loadTexture("floor.png");
    unsigned int modeTexID = Loader::Trexture::loadTexture("anime.png");

    std::vector<Texture> floorTextures = {{floorTexID, "texture_diffuse", "floor.png"}};
    std::vector<Texture> brickTextures = {{brickTexID, "texture_diffuse", "fl.png"}};
    //std::vector<Texture> modelTextures = {{brickTexID, "texture_diffuse", "fl.png"}};

    Material floorMaterial("shaders/basic_texture_shader.vert","shaders/basic_texture_shader.frag", floorTextures,depthMap);
    Material cubeMaterial("shaders/basic_texture_shader.vert","shaders/basic_texture_shader.frag", brickTextures,depthMap);
    //Material modelMaterial("shaders/basic_texture_shader.vert","shaders/basic_texture_shader.frag", modelTextures,depthMap);

    StaticMesh cubeMesh(std::vector<float>(std::begin(indexedCubeVertices), std::end(indexedCubeVertices)),
                        std::vector<unsigned int>(std::begin(cubeIndices), std::end(cubeIndices)),&cubeMaterial);
    StaticMesh planeMesh(std::vector<float>(std::begin(indexedPlaneVertices),std::end(indexedPlaneVertices)),
                         std::vector<unsigned int>(std::begin(planeIndices),std::end(planeIndices)),&floorMaterial);

    SceneObject floor(&planeMesh);
    SceneObject cube(&cubeMesh);
    cube.transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    cube.transform.scale = glm::vec3(0.5f);

    // Skydome skydome;
    // if (!skydome.Setup()) {
    //     std::cerr << "Chyba pri inicializaci skydome." << std::endl;
    //     glfwTerminate();
    //     return -1;
    // }
    Skybox skybox(faces1);

    ModelFBX model("assets/models/grenade/untitled.fbx");
    model.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model.setFallbackMetallic(0.1f);
    model.setFallbackSmoothness(0.3f);
    model.transform.position = glm::vec3(3.0f, -0.5f, 0.0f);
    model.transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    model.transform.scale    = glm::vec3(0.01f);

    ModelFBX model1("assets/models/grenade/untitled.fbx");
    model1.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model1.setFallbackMetallic(0.1f);
    model1.setFallbackSmoothness(0.3f);
    model1.transform.position = glm::vec3(-3.0f, -0.5f, 0.0f);
    model1.transform.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    model1.transform.scale    = glm::vec3(0.01f);

    for(int i=0;i<model.numAnimations();++i) std::cout << i << " anim " <<model.animationName(i)  << std::endl;
    //model.playAnimationByIndex(0);
    //model.playAnimationByName(\"Run");
    //model.stopAnimation();
    // for (auto val : ourModel.meshes[0].indices) {
    //     //std::cout << val << " ";
    // }/
    //================================================================dIRECTION LIGHT
    float rotationSpeed = 50.0f;
    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
    float lightSpeed = 1.0f;
    static bool autoLightMovement = false;
    glm::vec3 lightColor = glm::vec3(1.0f);
    float ambientStrength = 0.1f;
    //=======================main loop

    while (!glfwWindowShouldClose(window)) {

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        //============================================================================imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Scene settings");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (ImGui::Button("Change cube rotation direction")) {
            rotationSpeed *= -1.0f;
        }

        ImGui::Separator();
        ImGui::Text("Light control");
        ImGui::SliderFloat("Light X", &lightPos.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Light Y", &lightPos.y, 0.0f, 15.0f);
        ImGui::SliderFloat("Light Z", &lightPos.z, -10.0f, 10.0f);
        ImGui::Separator();
        ImGui::Text("Light settings");
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor));
        ImGui::SliderFloat("Ambient strength", &ambientStrength, 0.0f, 1.0f);
        ImGui::Checkbox("Auto light movement", &autoLightMovement);
        ImGui::End();

        //============================================================================input
        processInput(window);

        if (autoLightMovement) {
            lightPos.x = sin(glfwGetTime() * lightSpeed) * 3.0f;
            lightPos.z = cos(glfwGetTime() * lightSpeed) * 3.0f;
        }

        //  (lightSpaceMatrix)
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane = 1.0f, far_plane = 17.5f;
        float orthoSize = 20.0f;
        float lightX = lightPos.x;
        float lightZ = lightPos.z;
        //glm::vec3 cubePos = cube.transform.position;
        glm::vec3 lightTarget = glm::vec3(0.0f, 0.0f, 0.0f);//center

        lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize,near_plane, far_plane);

        lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;

        floorMaterial.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        cubeMaterial.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        model.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        model1.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        cube.transform.rotation.y = glfwGetTime() * rotationSpeed;
        model1.transform.rotation.y = glfwGetTime() * rotationSpeed;


        // --- 1.pass depth map for shadow
        //============================================================================draw shadows
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        floor.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        cube.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        model.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);
        model1.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);
        //============================================================================draw shadows
        // --- 2. pass color ---
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection =glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        float t = (float)glfwGetTime();
        model.updateAnimation(t);
        model1.updateAnimation(t);
        //============================================================================draw geometry
        glm::mat4 invProjection = glm::inverse(projection);
        glm::mat4 invView = glm::inverse(view);
        // ================================================================= //
        float time = static_cast<float>(glfwGetTime());
        glm::vec3 sunWorldPos = glm::vec3(
            3000.0f * cos(time * 0.1f),
            1500.0f,
            3000.0f * sin(time * 0.1f)
            );

        glm::vec3 directionToSun = glm::normalize(sunWorldPos);

        glDisable(GL_DEPTH_TEST);
        // skydome.Draw(invView, invProjection, directionToSun);
        skybox.Draw(view, projection);
        glEnable(GL_DEPTH_TEST);

        floor.Draw(view, projection, lightSpaceMatrix);
        cube.Draw(view, projection, lightSpaceMatrix);
        model.draw(view,projection, camera.Position);
        model1.draw(view,projection, camera.Position);

        //============================================================================draw imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}


//============================================================================input functiona
//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief processInput
/// \param window
///\
///
void processInput(GLFWwindow *window) {

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        if (!keyLWasPressed) { // Kontrola, aby se nepřepínalo neustále
            cursorEnabled = !cursorEnabled;
            if (cursorEnabled) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            keyLWasPressed = true;
        }
    } else {
        keyLWasPressed = false;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {

    ImGuiIO &io = ImGui::GetIO();

    if (io.WantCaptureMouse) {
        return;
    }
    if (cursorEnabled) {
        return;
    }
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}



#endif // DEFAULT_H
