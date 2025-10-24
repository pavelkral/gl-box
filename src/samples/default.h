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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "../glbox/Camera.h"
#include "../glbox/Model.h"
#include "../glbox/SceneObject.h"
#include "../glbox/StaticMesh.h"
#include "../glbox/PbrMaterial.h"
#include "../glbox/Transform.h"
#include "../glbox/Shader.h"
#include "../glbox/Texture.h"
#include "../glbox/ProceduralSky.h"
#include "../glbox/TexturedSky.h"
#include "../glbox/HdriSky.h"
#include "../glbox/geometry/Geometry.h"
#include "../glbox/physics/Raycast.h"
#include "../glbox/DebugDraw.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

/**
 * Toto je tvá nová, rychlá funkce pro raycasting.
 */
/**
 * Provede raycast proti scéně reprezentované Octree.
 * @param ray Paprsek (ve world-space).
 * @param sceneOctree Konstantní reference na Octree scény.
 * @param modelMatrices Konstantní reference na mapu modelových matic.
 * @param outHit Výstupní struktura pro výsledek nejbližšího zásahu.
 * @return True, pokud byl jakýkoliv objekt zasažen, jinak false.
 */
bool PerformRaycast(const Ray& ray,
                    const Octree& sceneOctree, // <-- Přidáno jako const reference
                    const std::map<StaticMesh*, glm::mat4>& modelMatrices, // <-- Přidáno jako const reference
                    RaycastHit& outHit)
{
    // Resetujeme výsledek
    outHit.hit = false;
    outHit.distance = FLT_MAX;
    outHit.object = nullptr;

    // 1. Získáme ze stromu jen relevantní objekty
    std::vector<StaticMesh*> potentialHits;
    sceneOctree.Query(ray, potentialHits); // <-- Používáme parametr

    if (potentialHits.empty()) {
        return false; // Paprsek netrefil žádný AABB
    }

    // 2. Teď provedeme test "brute-force", ALE UŽ JEN NA TĚCH PÁR KANDIDÁTECH
    for (StaticMesh* mesh : potentialHits) {

        // Bezpečné nalezení matice v mapě
        auto it = modelMatrices.find(mesh);
        if (it == modelMatrices.end()) {
            // Tento mesh nemá z nějakého důvodu matici v mapě, přeskočíme ho
            std::cerr << "CHYBA: Raycast nenašel modelovou matici pro mesh!" << std::endl;
            continue;
        }

        // Použijeme matici z parametru
        const glm::mat4& modelMatrix = it->second;

        // Znovu spočítáme world AABB pro přesný test
        // (Alternativa: Octree by mohl vracet i AABB, se kterým byl objekt vložen)
        BoxCollider worldAABB = mesh->localAABB.GetTransformed(modelMatrix);

        float t; // Vzdálenost zásahu
        if (worldAABB.Intersects(ray, t)) {

            // TODO: Zde by měl být přesný test proti trojúhelníkům.
            // Prozatím bereme zásah AABB jako finální zásah.

            // 3. Je tento zásah blíž než ten předchozí?
            if (t < outHit.distance) {
                outHit.hit = true;
                outHit.distance = t;
                outHit.point = ray.origin + ray.direction * t;
                outHit.object = mesh;
            }
        }
    }

    return outHit.hit;
}


const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
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
    Shader depthShader("shaders/depth.vert", "shaders/depth.frag");
    Shader modelDepthShader("shaders/model/model_depth.vert", "shaders/model/model_depth.frag");
    //============================================================================
    // ---Scene ---

    ProceduralSky skydome;
    skydome.Setup();

    std::vector<std::string> faces
        {
            "assets/textures/skybox/right.bmp",
            "assets/textures/skybox/left.bmp",
            "assets/textures/skybox/top.bmp",
            "assets/textures/skybox/bottom.bmp",
            "assets/textures/skybox/front.bmp",
            "assets/textures/skybox/back.bmp"
        };
    TexturedSky skybox(faces);

    HdriSky sky;
    sky.init("assets/textures/sky.hdr");

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<float> vertices1;
    std::vector<unsigned int> indices1;
    std::vector<float> vertices2;
    std::vector<unsigned int> indices2;

    Geometry::generatePlane(100.0f, 100.0f, 10, 10, 100.0f, 100.0f, vertices1,indices1);
    Geometry::generateCube(1.0f, vertices, indices);
    Geometry::generateSphere(0.5f, 32, 32, vertices2, indices2);

    unsigned int floorTexID = Trexture::loadTexture("assets/textures/floor.png");
    unsigned int floorTexNormID = Trexture::loadTexture("assets/textures/floorN.png");
    unsigned int floorTexRoughID = Trexture::loadTexture("assets/textures/floorM.png");

    unsigned int albedoTex = Trexture::loadTexture("assets/textures/clamp/base.png");
    unsigned int normalTex = Trexture::loadTexture("assets/textures/clamp/norm.png");
    unsigned int metallicTex = Trexture::loadTexture("assets/textures/clamp/met.png");
    unsigned int roughnessTex = Trexture::loadTexture("assets/textures/clamp/ro.png");

    // Parameters:
    // glm::vec3 color          → Base color of the material (albedo, sRGB)
    // float alpha              → Opacity (1.0 = fully opaque, 0.0 = fully transparent)
    // float metallic           → Metalness (0.0 = non-metal/plastic, 1.0 = fully metallic)
    // float roughness          → Surface roughness (0.0 = mirror-like, 1.0 = fully rough/matte)
    // float ao                 → Ambient occlusion / strength of ambient light (0.0–1.0)
    // float reflectionStrength → Strength of reflections (0.0–1.0), how much the environment map affects appearance
    // float transmission       → Transparency for refraction/glass (0.0 = none, 1.0 = fully transparent)
    // float indexOfRefraction  → Index of refraction for transparent materials (glass), default 1.52

    glm::vec3 albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float alpha = 1.0f;
    float metallic = 1.0f;;
    float roughness = 0.1f;;
    float ao = 1.0f;;
    float reflectionStrength = 1.0f;;
    float transmission= 0.0f;;
    float ior = 1.0f;;

    PbrMaterial goldMaterial;
    goldMaterial.metallic = 1.0f;
    goldMaterial.roughness = 0.4f;
   // goldMaterial.setAlbedoMap( albedoTex);
   // goldMaterial.setNormalMap(normalTex );
   // goldMaterial.setMetallicMap(metallicTex);

    PbrMaterial goldMaterial1;
    goldMaterial1.metallic = 1.0f;
    goldMaterial1.roughness = 0.4f;
    goldMaterial1.reflectionStrength = 0;
    goldMaterial1.ao = 0.0f;
    goldMaterial1.setAlbedoMap( floorTexID);
    goldMaterial1.setNormalMap(floorTexNormID);
    goldMaterial1.setMetallicMap(floorTexRoughID);

    StaticMesh staticmesh(vertices2,indices2, &goldMaterial);
    SceneObject pbrcube(&staticmesh);
    pbrcube.transform.scale = glm::vec3(1.5f);
    pbrcube.transform.position = glm::vec3(1.0f, 0.5f, 2.0f);

    StaticMesh planeMesh(vertices1,indices1,&goldMaterial1);
    SceneObject floor(&planeMesh);
    floor.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);

    StaticMesh cubeMesh1(vertices2,indices2,&goldMaterial1);
    SceneObject cube(&cubeMesh1);
    cube.transform.position = glm::vec3(-1.0f, 0.5f, 2.0f);
    cube.transform.scale = glm::vec3(1.5f);

    ModelFBX model("assets/models/Player/Player.fbx");
    unsigned int myAlbedoTex = Trexture::loadTexture("assets/models/Player/Textures/Player_D.tga");
    unsigned int myNormalTex = Trexture::loadTexture("assets/models/Player/Textures/Player_NRM.tga");
    unsigned int myMetallicTex = Trexture::loadTexture("assets/models/Player/Textures/Player_M.tga");
    unsigned int mySmoothnessTex = Trexture::loadTexture("assets/models/Player/Textures/Gun_D.tga");
    model.setAlbedoTexture(myAlbedoTex,0);
    model.setNormalTexture(myNormalTex,0);
    model.setMetallicTexture(myMetallicTex,0);
    model.setAlbedoTexture(mySmoothnessTex,1);
    model.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model.setFallbackMetallic(1.1f);
    model.setFallbackSmoothness(0.3f);
    model.transform.position = glm::vec3(3.0f, -0.5f, 0.0f);
    model.transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    model.transform.scale    = glm::vec3(0.01f);
    SceneObject soldier1(&model);

    ModelFBX model1("assets/models/USMarines/usmarine.FBX");
    unsigned int Marine =Trexture::loadTexture("assets/models/USMarines/usmarine-01.jpg");
    unsigned int m16 = Trexture::loadTexture("assets/models/USMarines/m16.jpg");
    model1.setFallbackAlbedo(0.7f, 0.7f, 0.75f);
    model1.setFallbackMetallic(0.1f);
    model1.setFallbackSmoothness(0.3f);
    model1.transform.position = glm::vec3(-3.0f, -0.5f, 0.0f);
    // model1.transform.rotation = glm::vec3(90.0f, 180.0f, 180.0f);
    model1.transform.rotation = glm::vec3(-90.0f, 180.0f, 0.0f);
    model1.transform.scale    = glm::vec3(0.012f);
    model1.setAlbedoTexture(m16,1);
    model1.setAlbedoTexture(Marine,0);
    SceneObject soldier(&model);
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
    glm::vec3 lightColor = glm::vec3(4.0f);
    float ambientStrength = 0.3f;

    using Clock = std::chrono::steady_clock;
    auto lastUpdate = Clock::now();
    const std::chrono::seconds updateInterval(10);
    bool sphere = true;

    //===============================================================================================
    std::vector<StaticMesh*> allMeshes;
    allMeshes.emplace_back(&planeMesh);
    allMeshes.emplace_back(&cubeMesh1);
    allMeshes.emplace_back(&staticmesh);

    // Vytvoření a inicializace mapy s maticemi
    std::map<StaticMesh*, glm::mat4> modelMatrices;

    // 1. Definuj ZÁKLADNÍ transformace a ulož je do mapy
    // Pozn.: Pro cubemeshe1 a staticmesh (ta co se hýbe) nastav ty polohy,
    // které mají mít při spuštění.
    modelMatrices[&planeMesh] = glm::mat4(1.0f); // Jednotková matice pro podlahu/plane
    modelMatrices[&cubeMesh1] = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 0.5f, 0.0f));
    modelMatrices[&staticmesh] = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.5f, 0.0f));

    // 2. Sestavení Octree (BuildSceneOctree by byla lepší volba, ale teď to necháme inline)
    Octree sceneOctree(BoxCollider(glm::vec3(0.0f), glm::vec3(0.0f)));
    std::map<StaticMesh*, BoxCollider> allWorldAABBs;

    // Zde už mapa modelMatrices NENÍ prázdná a Octree se sestaví správně
    for (StaticMesh* mesh : allMeshes) {
        const glm::mat4& modelMatrix = modelMatrices[mesh];
        BoxCollider worldAABB = mesh->localAABB.GetTransformed(modelMatrix);
        allWorldAABBs[mesh] = worldAABB;
    }

    sceneOctree.Build(allWorldAABBs);
    std::cout << "Octree builed!" << std::endl;
    DebugDraw debugDrawer;

    //===========================================================================main loop
    //==================================================================================

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

        ImGui::Separator();
        ImGui::Text("Gold Material Parameters");
        ImGui::ColorEdit3("Albedo Color", glm::value_ptr(albedoColor));
        ImGui::SliderFloat("Alpha (Opacity)", &alpha, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Ambient Occlusion", &ao, 0.0f, 1.0f);
        ImGui::SliderFloat("Reflection Strength", &reflectionStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Transmission", &transmission, 0.0f, 1.0f);
        ImGui::SliderFloat("Index of Refraction (IOR)", &ior, 1.0f, 2.5f);

        ImGui::End();
        //============================================================================input
        processInput(window);

        if (elapsed >= updateInterval){
            if (sphere)
                Geometry::generateSphere(0.5f, 32, 32, vertices, indices);
            else
                Geometry::generateCube(1.0f, vertices, indices);

            staticmesh.UpdateGeometry(vertices, indices);
            cubeMesh1.UpdateGeometry(vertices, indices);

            sphere = !sphere;
            lastUpdate = now;
        }
        if (autoLightMovement) {
            lightPos.x = sin(glfwGetTime() * lightSpeed) * 3.0f;
            lightPos.z = cos(glfwGetTime() * lightSpeed) * 3.0f;
        }

        glm::mat4 projection =glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        // Nastavíme rayLength na 20.0f, jak jste testoval
        float rayLength = 20.0f;
        Ray myRay(camera.Position, camera.Front);
        RaycastHit hitResult;
        //===============================================================================================
        // Zde probíhá POUZE výpočet raycastu, kreslení přesouváme níže!
        //===============================================================================================
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

        // glm::mat4 projection =glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        float t = (float)glfwGetTime();

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

        cube.transform.rotation.y = glfwGetTime() * rotationSpeed;
        // --- 1.pass depth map for shadow
        //============================================================================draw shadows
        glViewport(0, 0, shadowMap.width, shadowMap.height);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
        glClear(GL_DEPTH_BUFFER_BIT);

        floor.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        cube.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        model.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);
        model1.DrawForShadow(modelDepthShader.ID, lightSpaceMatrix);
        pbrcube.DrawForShadow(depthShader.ID, lightSpaceMatrix);
        //staticmesh.DrawForShadow(depthShader.ID,modelA, lightSpaceMatrix);

        //============================================================================draw shadows
        // --- 2. pass color ---
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        model.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        model1.setLightProperties(lightPos, lightColor, ambientStrength,camera.Position);
        //  model.playAnimationByIndex(0);
        model.setAnimationLoopRange(3.5f, 3.55f);
        model.updateAnimation(t);
        model1.updateAnimation(t);
        model.disableAnimationLoopRange();

        goldMaterial.setParameters(albedoColor, alpha, metallic, roughness, ao, reflectionStrength, transmission, ior);
        //============================================================================draw geometry

        glDisable(GL_DEPTH_TEST);
        float time1 = static_cast<float>(glfwGetTime());
        skydome.Draw(invView, invProjection, directionToSun,time1);
        //skybox.Draw(view, projection);
        //sky.draw(view, projection);
        glEnable(GL_DEPTH_TEST);

        glm::vec3 objPos;
        glm::vec3 lightDir;
        unsigned int cubeMap = sky.getCubeMap();

        objPos       = glm::vec3(floor.transform.position);
        lightDir     = glm::normalize(lightPos - objPos);

        floor.Draw(view, projection, camera.Position, cubeMap, shadowMap.texture,lightSpaceMatrix, lightDir,lightColor);
        cube.Draw(view, projection, camera.Position, cubeMap, shadowMap.texture,lightSpaceMatrix, lightDir,lightColor);
        model.draw(view,projection, camera.Position);
        model1.draw(view,projection, camera.Position);

        objPos       = glm::vec3(pbrcube.transform.position);
        lightDir     = glm::normalize(lightPos - objPos);

        pbrcube.Draw(view, projection, camera.Position, cubeMap, shadowMap.texture,lightSpaceMatrix, lightDir,lightColor);

        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        //============================================================================DRAW DEBUG LINES (NEW LOCATION)
        // Kreslení se provádí TADY, po vyčištění bufferu a vykreslení scény.

        // Zakázat testování hloubky, aby byly debug čáry vždy viditelné před geometrií.
        glDisable(GL_DEPTH_TEST);

        // --- TESTovací čára pro ověření funkčnosti DebugDraweru (ŽLUTÁ) ---
        // Tuto čáru byste měl vidět.
        debugDrawer.DrawLine(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), view, projection);

        // --- DYNAMICKÁ TESTovací čára (MODRÁ, KRESLENÁ NAHORU) ---
        // Nyní kreslíme čáru kolmo k pohledu, abychom zaručili viditelnost.
        glm::vec3 drawStartPoint = myRay.origin + myRay.direction * 1.0f;
        // Cíl posunutý o 0.5f nahoru ve world space od startovního bodu.
        glm::vec3 dynamicTestPoint = drawStartPoint + camera.Up * 0.5f;
        debugDrawer.DrawLine(drawStartPoint, dynamicTestPoint, glm::vec3(0.0f, 0.0f, 1.0f), view, projection);

        std::cout << "Dynamický startovní bod: " << glm::to_string(drawStartPoint)
                  << ", Zkušební bod (modrý, 0.5f nahoru): " << glm::to_string(dynamicTestPoint) << std::endl;

        // --- Kreslení Raycast Debug Čáry (ČERVENÁ/ZELENÁ) ---
        if (PerformRaycast(myRay, sceneOctree, modelMatrices, hitResult)) {
            // Raycast našel objekt. Nyní zkontrolujeme, zda je v požadovaném dosahu.
            if (hitResult.distance < rayLength) {
                // Skutečný zásah v dosahu (Kreslíme zeleně k bodu zásahu)
                glm::vec3 hitColor = glm::vec3(0.0f, 1.0f, 0.0f); // Zelená pro zásah
                // Kreslíme čáru od posunutého startovního bodu (drawStartPoint) k bodu zásahu (hitResult.point)
                debugDrawer.DrawLine(drawStartPoint, hitResult.point, hitColor, view, projection);
                std::cout << "Hit! Objekt: " << hitResult.object << ", Lenght: " << hitResult.distance
                          << ", Start: " << glm::to_string(drawStartPoint)
                          << ", Hit: " << glm::to_string(hitResult.point) << std::endl;
            } else {
                // Zásah proběhl, ale je příliš daleko (Kreslíme červeně jen do max. délky)
                // Koncový bod raycastu, začíná od myRay.origin, ale kreslí se od drawStartPoint
                glm::vec3 endPoint = myRay.origin + myRay.direction * rayLength;
                glm::vec3 missColor = glm::vec3(1.0f, 0.0f, 0.0f); // Červená pro miss (příliš daleko)
                // Kreslíme čáru od posunutého startovního bodu (drawStartPoint) k maximálnímu bodu (endPoint)
                debugDrawer.DrawLine(drawStartPoint, endPoint, missColor, view, projection);
                std::cout << "Miss (Too far). Ray end at: " << glm::to_string(endPoint)
                          << ". Nearest object at: " << hitResult.distance
                          << ", Start: " << glm::to_string(drawStartPoint) << std::endl;
            }
        }
        else {
            // Celkový Miss: Čáru kreslíme na celou délku paprsku (červeně)
            glm::vec3 endPoint = myRay.origin + myRay.direction * rayLength;
            glm::vec3 missColor = glm::vec3(1.0f, 0.0f, 0.0f); // Červená pro miss
            // Kreslíme čáru od posunutého startovního bodu (drawStartPoint) k maximálnímu bodu (endPoint)
            debugDrawer.DrawLine(drawStartPoint, endPoint, missColor, view, projection);
            std::cout << "Miss. Ray end at: " << glm::to_string(endPoint)
                      << ", Start: " << glm::to_string(drawStartPoint) << std::endl;
        }

        glEnable(GL_DEPTH_TEST);
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


//===================================input functiona==========================================
//////////////////////////////////////////////////////////////////////////////////////////////


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
