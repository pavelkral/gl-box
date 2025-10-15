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

#include "../glbox/Camera.h"
#include "../glbox/Material.h"
#include "../glbox/Model.h"
#include "../glbox/SceneObject.h"
#include "../glbox/ProceduralMesh.h"
#include "../glbox/Transform.h"
#include "../glbox/Shader.h"
#include "../glbox/Texture.h"
#include "../glbox/ProceduralSky.h"
#include "../glbox/TexturedSky.h"
#include "../glbox/HdriSky.h"
#include "../glbox/geometry/Sphere.h"
#include "../glbox/geometry/Geometry.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
bool cursorEnabled = false;
bool keyLWasPressed = false;
Camera camera(glm::vec3(0.0f, 3.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main() {

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
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

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version:   " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Renderer:       " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Vendor:         " << glGetString(GL_VENDOR) << std::endl;

    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    std::cout << "OpenGL numeric version: " << major << "." << minor << std::endl;

    //============================================================================
    //=====================================================================seetup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- DEPTH BUFFER SHADOWS ---
    DepthMap shadowMap = Trexture::createDepthMapFBO();
    //============================================================================
    // ---Scene ---
    Shader depthShader("shaders/depth.vert", "shaders/depth.frag");
    Shader modelDepthShader("shaders/model/model_depth.vert", "shaders/model/model_depth.frag");

    std::vector<std::string> faces1
        {
            "assets/textures/skybox/right.bmp",
            "assets/textures/skybox/left.bmp",
            "assets/textures/skybox/top.bmp",
            "assets/textures/skybox/bottom.bmp",
            "assets/textures/skybox/front.bmp",
            "assets/textures/skybox/back.bmp"
        };

    unsigned int floorTexID = Trexture::loadTexture("assets/textures/floor.png");
    unsigned int brickTexID = Trexture::loadTexture("assets/textures/floor.png");


    std::vector<Texture> floorTextures = {{floorTexID, "texture_diffuse", "floor.png"}};
    std::vector<Texture> brickTextures = {{brickTexID, "texture_diffuse", "fl.png"}};
    //std::vector<Texture> modelTextures = {{brickTexID, "texture_diffuse", "fl.png"}};

    Material floorMaterial("shaders/material/texture.vert","shaders/material/texture.frag", floorTextures,shadowMap.texture);
    Material cubeMaterial("shaders/material/texture.vert","shaders/material/texture.frag", brickTextures,shadowMap.texture);

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<float> vertices1;
    std::vector<unsigned int> indices1;

    Geometry::generatePlane(100.0f, 100.0f, 10, 10, 100.0f, 100.0f, vertices1,indices1);
    Geometry::generateCube(1.0f, vertices, indices);
   // Geometry::generateSphere(1.0f, 32, 32, vertices, indices);
    ProceduralMesh cubeMesh(vertices,indices,&cubeMaterial);
    ProceduralMesh planeMesh(vertices1,indices1,&floorMaterial);

    SceneObject floor(&planeMesh);
    SceneObject cube(&cubeMesh);
    cube.transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    cube.transform.scale = glm::vec3(0.5f);
    floor.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);

    ProceduralSky skydome;
    if (!skydome.Setup()) {
        std::cerr << "err skydome." << std::endl;
        glfwTerminate();
        return -1;
    }

    TexturedSky skybox(faces1);
    \
    HdriSky sky;
    sky.init("assets/textures/sky.hdr");

    Sphere sphereLeft;
    Sphere sphereRight;
    Sphere sphereCenter;

    unsigned int albedoTex = Trexture::loadTexture("assets/textures/clamp/base.png");
    unsigned int normalTex = Trexture::loadTexture("assets/textures/clamp/norm.png");
    unsigned int metallicTex = Trexture::loadTexture("assets/textures/clamp/met.png");
    unsigned int roughnessTex = Trexture::loadTexture("assets/textures/clamp/ro.png");

    sphereLeft.setAlbedoTexture(albedoTex);
    sphereLeft.setNormalTexture(normalTex);
 ///   sphereLeft.setMetallicTexture(metallicTex);
    sphereLeft.setRoughnessTexture(roughnessTex);

    ModelFBX model("assets/models/Player/Player.fbx");
    GLuint myAlbedoTex = Trexture::loadTexture("assets/models/Player/Textures/Player_D.tga");
    GLuint myNormalTex = Trexture::loadTexture("assets/models/Player/Textures/Player_NRM.tga");
    GLuint myMetallicTex = Trexture::loadTexture("assets/models/Player/Textures/Player_M.tga");
    GLuint mySmoothnessTex = Trexture::loadTexture("assets/models/Player/Textures/Gun_D.tga");
    model.setAlbedoTexture(myAlbedoTex,0);
    model.setNormalTexture(myNormalTex,0);
    model.setMetallicTexture(myMetallicTex,0);
    model.setAlbedoTexture(mySmoothnessTex,1);
    model.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model.setFallbackMetallic(0.1f);
    model.setFallbackSmoothness(0.3f);
    model.transform.position = glm::vec3(3.0f, -0.5f, 0.0f);
    model.transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    model.transform.scale    = glm::vec3(0.01f);

    ModelFBX model1("assets/models/USMarines/usmarine.FBX");
    GLuint Marine =Trexture::loadTexture("assets/models/USMarines/usmarine-01.jpg");
    GLuint m16 = Trexture::loadTexture("assets/models/USMarines/m16.jpg");
    model1.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model1.setFallbackMetallic(0.1f);
    model1.setFallbackSmoothness(0.3f);
    model1.transform.position = glm::vec3(-3.0f, -0.5f, 0.0f);
   // model1.transform.rotation = glm::vec3(90.0f, 180.0f, 180.0f);
    model1.transform.rotation = glm::vec3(-90.0f, 180.0f, 0.0f);
    model1.transform.scale    = glm::vec3(0.012f);
    model1.setAlbedoTexture(m16,1);
    model1.setAlbedoTexture(Marine,0);
    //for(int i=0;i<model.numAnimations();++i) std::cout << i << " anim " <<model.animationName(i)  << std::endl;
    //model1.stopAnimation();
   // model.stopAnimation();
 //   model.playAnimationByIndex(1);
    //model.stopAnimation();
    // for (auto val : ourModel.meshes[0].indices) {
    //     //std::cout << val << " ";
    // }/

    //================================================================DIRECTION LIGHT

    float rotationSpeed = 50.0f;  
    glm::vec3 lightPos(-2.0f, 14.0f, -1.0f);
    float lightSpeed = 1.0f;
    static bool autoLightMovement = false;
    glm::vec3 lightColor = glm::vec3(1.0f);
    float ambientStrength = 0.3f;

    using Clock = std::chrono::steady_clock;
    auto lastUpdate = Clock::now();
    const std::chrono::seconds updateInterval(5);
    bool sphere = true;

    //===========================================================================main loop
    //==================================================================================.
    while (!glfwWindowShouldClose(window)) {

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        auto now = Clock::now();
        auto elapsed = now - lastUpdate;
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
        ImGui::SliderFloat("Light X", &lightPos.x, -40.0f, 40.0f);
        ImGui::SliderFloat("Light Y", &lightPos.y, 0.0f, 40.0f);
        ImGui::SliderFloat("Light Z", &lightPos.z, -40.0f, 40.0f);
        ImGui::Separator();
        ImGui::Text("Light settings");
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor));
        ImGui::SliderFloat("Ambient strength", &ambientStrength, 0.0f, 1.0f);
        ImGui::Checkbox("Auto light movement", &autoLightMovement);
        ImGui::End();

        //============================================================================input
        processInput(window);

        if (elapsed >= updateInterval){
            if (sphere)
                Geometry::generateSphere(1.0f, 32, 32, vertices, indices);
            else
                Geometry::generateCube(2.0f, vertices, indices);

            cube.mesh->UpdateGeometry(vertices, indices);
            sphere = !sphere;
            lastUpdate = now;
        }
        if (autoLightMovement) {
            lightPos.x = sin(glfwGetTime() * lightSpeed) * 3.0f;
            lightPos.z = cos(glfwGetTime() * lightSpeed) * 3.0f;
        }


        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane = 1.0f, far_plane = 50.0f;
        float orthoSize =20.0f;
        float lightX = lightPos.x;
        float lightZ = lightPos.z;
        glm::vec3 lightTarget = glm::vec3(0.0f, 0.0f, 0.0f);//center

        lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize,near_plane, far_plane);
        lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;

        floorMaterial.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        cubeMaterial.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        model.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        model1.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        cube.transform.rotation.y = glfwGetTime() * rotationSpeed;
        glm::mat4 projection =glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);       
        glm::mat4 view = camera.GetViewMatrix();
        float t = (float)glfwGetTime();

        glm::mat4 modelA = glm::mat4(1.0f);
        modelA = glm::translate(modelA, glm::vec3(-1.5f, 0.0f, 2.0f));
        modelA = glm::scale(modelA, glm::vec3(0.5f));
        glm::mat4 modelB = glm::mat4(1.0f);
        modelB = glm::translate(modelB, glm::vec3(0.0f, 0.0f, 2.0f));
        modelB = glm::scale(modelB, glm::vec3(0.5f));
        glm::mat4 modelC = glm::mat4(1.0f);
        modelC = glm::translate(modelC, glm::vec3(1.5f, 0.0f, 2.0f));
        modelC = glm::scale(modelC, glm::vec3(0.5f));
         const float IOR_GLASS = 1.0f / 1.52f;
      //  model1.transform.rotation.y = glfwGetTime() * rotationSpeed;
         glm::mat4 invProjection = glm::inverse(projection);
         glm::mat4 invView = glm::inverse(view);
         // ================================================================= //
         float time = static_cast<float>(glfwGetTime());
         glm::vec3 sunWorldPos = glm::vec3(
             30.0f * cos(time * 0.1f),
             15.0f,
             30.0f * sin(time * 0.1f)
             );
         // lightPos.x = sunWorldPos.x;
         //  lightPos.z = sunWorldPos.z;
         // lightPos.y = sunWorldPos.y;

         glm::vec3 directionToSun = glm::normalize(sunWorldPos);

        // --- 1.pass depth map for shadow
        //============================================================================draw shadows
        glViewport(0, 0, shadowMap.width, shadowMap.height);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
        glClear(GL_DEPTH_BUFFER_BIT);

        floor.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        cube.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        model.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);
        model1.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);

        sphereLeft.drawForShadow(depthShader.ID, modelA, lightSpaceMatrix);
        sphereCenter.drawForShadow(depthShader.ID, modelB, lightSpaceMatrix);
        sphereRight.drawForShadow(depthShader.ID, modelC, lightSpaceMatrix);
        //============================================================================draw shadows
        // --- 2. pass color ---
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      //  model.playAnimationByIndex(0);
        model.setAnimationLoopRange(3.5f, 3.55f);
        model.updateAnimation(t);
        model1.updateAnimation(t);
        model.disableAnimationLoopRange();
        //============================================================================draw geometry

        glDisable(GL_DEPTH_TEST);
      //  skydome.Draw(invView, invProjection, directionToSun);
        //skybox.Draw(view, projection);
        sky.draw(view, projection);
        glEnable(GL_DEPTH_TEST);

        unsigned int cubeMap = sky.getCubeMap();

        floor.Draw(view, projection, lightSpaceMatrix);
        cube.Draw(view, projection, lightSpaceMatrix);
        model.draw(view,projection, camera.Position);
        model1.draw(view,projection, camera.Position);

        // Sets the material properties for a Sphere object
        // Parameters:
        // glm::vec3 color          → Base color of the material (albedo, sRGB)
        // float alpha              → Opacity (1.0 = fully opaque, 0.0 = fully transparent)
        // float metallic           → Metalness (0.0 = non-metal/plastic, 1.0 = fully metallic)
        // float roughness          → Surface roughness (0.0 = mirror-like, 1.0 = fully rough/matte)
        // float ao                 → Ambient occlusion / strength of ambient light (0.0–1.0)
        // float reflectionStrength → Strength of reflections (0.0–1.0), how much the environment map affects appearance
        // float transmission       → Transparency for refraction/glass (0.0 = none, 1.0 = fully transparent)
        // float indexOfRefraction  → Index of refraction for transparent materials (glass), default 1.52
        sphereLeft.setMaterial(glm::vec3(1.0f,0.0f,0.0f), 1.0f, 0.0f, 0.1f, 1.0f, 0.5f);
        sphereRight.setMaterial(glm::vec3(0.08f,1.0f,0.08f), 1.0f, 1.0f, 0.3f, 0.01f, 1.0f);
        sphereCenter.setMaterial(glm::vec3(1.0f,1.0f,1.0f), 1.0f, 1.0f, 0.1f, 1.0f, 1.0f);

        glm::vec3 objPos;
        glm::vec3 lightDir;
        //glm::vec3 lightColor = glm::vec3(300.0f, 300.0f, 300.0f); //  Direct Lighting)
        objPos     = glm::vec3(modelA[3]);
        lightDir   = glm::normalize(lightPos - objPos);
        sphereLeft.draw(modelA, view, projection, camera.Position, cubeMap, shadowMap.texture,
                        lightSpaceMatrix, lightDir,lightColor);
        objPos     = glm::vec3(modelB[3]);
        lightDir   = glm::normalize(lightPos - objPos);

        sphereRight.draw(modelB, view, projection, camera.Position, cubeMap, shadowMap.texture,
                         lightSpaceMatrix, lightDir,lightColor);
        objPos     = glm::vec3(modelC[3]);
        lightDir   = glm::normalize(lightPos - objPos);

        sphereCenter.draw(modelC, view, projection, camera.Position, cubeMap, shadowMap.texture,
                          lightSpaceMatrix, lightDir,lightColor);

       // glDepthMask(GL_FALSE);
       // glEnable(GL_BLEND);
       // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

       // glDisable(GL_BLEND);
       // glDepthMask(GL_TRUE);

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





