#ifndef ECS_H
#define ECS_H

#include <iostream>
#include <vector>
//#include <algorithm>
#include <cmath>
//#include <numeric>
#include <cstdlib>
#include <ctime>

// Pro M_PI (pokud není definováno)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// OpenGL závislosti
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- GLOBÁLNÍ DATA PRO KAMERU A VSTUPY ---

// Rozměry okna
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Data kamery
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 25.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw   = -90.0f; // Počáteční rotace kolem Y (aby se kamera dívala dopředu)
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float cameraSpeed = 5.0f; // Rychlost pohybu v jednotkách/sekundu

// Callback funkce pro myš
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos; // Obráceno, protože Y souřadnice jdou dolů
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    // Omezení úhlu náklonu (Pitch)
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // Vypočet nového vektoru cameraFront
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Zpracování klávesnice pro pohyb
void processInput(GLFWwindow *window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float velocity = cameraSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += velocity * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= velocity * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
}


// --- OpenGL Pomocné Funkce a Shadery ---

void checkShaderCompileErrors(GLuint shader, const std::string& type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "CHYBA::SHADER::" << type << "::KOMPILACE_SELHALA\n" << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "CHYBA::PROGRAM::LINKOVANI_SELHALO\n" << infoLog << std::endl;
        }
    }
}

// Upravený Vertex Shader pro příjem instančních dat MODEL matice a BARVY
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in mat4 aModel;
layout (location = 5) in vec4 aColor;

uniform mat4 view;
uniform mat4 projection;

out vec4 vColor;

void main()
{
    gl_Position = projection * view * aModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)glsl";

// Fragment Shader pro vykreslení barvy z ColorComponent
const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vColor;
}
)glsl";

// --- 1. KOMPONENTY (Čistá data) ---

using EntityID = unsigned int;

struct PositionComponent { float x, y, z; };
struct RotationComponent { float angle, axisX, axisY, axisZ; };
struct VelocityComponent { float dx, dy, dz; };
struct RenderComponent { unsigned int vaoID; };
struct ColorComponent { float r, g, b, a = 1.0f; };
struct MovementTypeComponent { int type; float speed; };

struct WorldMatrixComponent {
    glm::mat4 matrix{1.0f};
};

// --- 2. ECS JÁDRO: Entity Manager ---

class EntityManager {
public:
    std::vector<PositionComponent> positions;
    std::vector<RotationComponent> rotations;
    std::vector<VelocityComponent> velocities;
    std::vector<RenderComponent> renders;
    std::vector<WorldMatrixComponent> worldMatrices;
    std::vector<ColorComponent> colors;
    std::vector<MovementTypeComponent> movementTypes;

    std::vector<EntityID> entities;

private:
    EntityID nextID = 0;

public:
    EntityID createEntity() {
        EntityID id = nextID++;
        entities.push_back(id);
        positions.emplace_back();
        rotations.emplace_back();
        velocities.emplace_back();
        renders.emplace_back();
        worldMatrices.emplace_back();
        colors.emplace_back();
        movementTypes.emplace_back();

        return id;
    }

    void destroyEntity(EntityID id) {
        std::cout << "Entita " << id << " zničena (v tomto příkladu jen ignorována)." << std::endl;
    }
};

// --- 3. SYSTÉMY (Logika zpracování dat) ---

class TransformSystem {
public:
    void update(float deltaTime, EntityManager& em) {
        static float globalTime = 0.0f;
        globalTime += deltaTime;

        const float radius = 10.0f;
        const float rotationSpeed = 0.5f;
        const float sineAmplitude = 5.0f;

        for (size_t i = 0; i < em.entities.size(); ++i) {

            if (em.movementTypes[i].type == 1) { // Kruhová rotace
                float initialAngle = (float)em.entities[i] * (2.0f * M_PI / em.entities.size());
                float currentAngle = initialAngle + globalTime * rotationSpeed;

                em.positions[i].x = std::cos(currentAngle) * radius;
                em.positions[i].z = std::sin(currentAngle) * radius;
            }
            else if (em.movementTypes[i].type == 2) { // Sinusový pohyb
                em.positions[i].y = std::sin(globalTime * em.movementTypes[i].speed) * sineAmplitude;
            }

            // Vlastní rotace krychle
            em.rotations[i].angle += 2.0f * deltaTime;

            // Vytvoření lokální matice (Model)
            glm::mat4 model = glm::mat4(1.0f);

            // 1. Posun
            model = glm::translate(model, glm::vec3(em.positions[i].x, em.positions[i].y, em.positions[i].z));

            // 2. Rotace
            model = glm::rotate(model, em.rotations[i].angle,
                                glm::vec3(em.rotations[i].axisX, em.rotations[i].axisY, em.rotations[i].axisZ));

            em.worldMatrices[i].matrix = model;
        }
    }
};

class RenderSystem {
private:
    GLuint shaderProgram;
    GLuint cubeVAO;
    GLuint instanceVBO;

public:
    RenderSystem(GLuint sh, GLuint vao) : shaderProgram(sh), cubeVAO(vao) {
        glGenBuffers(1, &instanceVBO);
    }

    void update(EntityManager& em, const glm::mat4& view, const glm::mat4& projection) {
        if (em.entities.empty()) return;

        // --- 1. PŘÍPRAVA DAT (Model Matice) ---
        std::vector<glm::mat4> modelMatrices;
        modelMatrices.reserve(em.entities.size());
        for (const auto& wmc : em.worldMatrices) {
            modelMatrices.push_back(wmc.matrix);
        }

        // --- 2. PŘÍPRAVA DAT (Barvy) ---
        std::vector<glm::vec4> colors;
        colors.reserve(em.entities.size());
        for (const auto& cc : em.colors) {
            colors.push_back(glm::vec4(cc.r, cc.g, cc.b, cc.a));
        }

        // --- 3. VYKRESLOVÁNÍ (Instancing) ---
        glUseProgram(shaderProgram);

        // Uniforms
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        GLuint colorVBO;
        glGenBuffers(1, &colorVBO);

        // Nastavení atributů 1, 2, 3, 4 pro Model Matice
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(cubeVAO);

        size_t vec4Size = sizeof(glm::vec4);
        for (unsigned int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(i * vec4Size));
            glVertexAttribDivisor(1 + i, 1);
        }

        // Nastavení atributu 5 pro Barvy
        glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), colors.data(), GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
        glVertexAttribDivisor(5, 1);

        // Vykreslení všech krychlí jediným voláním
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, modelMatrices.size());

        // Čištění
        glDisableVertexAttribArray(5);
        for (unsigned int i = 0; i < 4; i++) {
            glDisableVertexAttribArray(1 + i);
        }
        glBindVertexArray(0);
        glDeleteBuffers(1, &colorVBO);
    }
};

// --- Hlavní funkce, Inicializace ---

float cubeVertices[] = {
    // Pozice (X, Y, Z) pro všech 36 vrcholů
    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
    0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

    -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,

    0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
    0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,

    -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
};

void initCubeMesh(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

int main() {
    // 1. GLFW a GLAD Inicializace
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Nastavení požadovaného rozlišení 1280x720
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "ECS OpenGL Krychle (FPS Camera)", NULL, NULL);
    if (!window) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // Nastavení callbacku pro myš a skrytí kurzoru
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; return -1; }
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT); // Nastavení viewportu

    // 2. Mesh a Shadery
    GLuint cubeVAO, cubeVBO;
    initCubeMesh(cubeVAO, cubeVBO);

    // ******* KOMPILACE SHADERŮ *******
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkShaderCompileErrors(vertexShader, "VERTEX");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkShaderCompileErrors(fragmentShader, "FRAGMENT");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkShaderCompileErrors(shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    // **********************************

    // 3. Inicializace ECS
    EntityManager entityManager;
    srand(static_cast<unsigned int>(time(0)));

    // --- Vytvoření 100 entit ---
    const int totalEntities = 100;
    const int numLayers = 10;
    const float layerSpacing = 1.5f;

    for (int i = 0; i < totalEntities; ++i) {
        EntityID id = entityManager.createEntity();

        if (i < 50) { // Kruhová rotace
            entityManager.movementTypes[id].type = 1;
            entityManager.movementTypes[id].speed = 1.0f;
            entityManager.positions[id].y = (float)(i % numLayers) * layerSpacing - 7.5f;
            entityManager.positions[id].x = 0.0f;
            entityManager.positions[id].z = 0.0f;
        } else { // Sinusový pohyb
            entityManager.movementTypes[id].type = 2;
            entityManager.movementTypes[id].speed = 2.0f + ((float)rand() / RAND_MAX) * 1.0f;
            entityManager.positions[id].x = (float)((i - 50) / 10) * 1.5f + 15.0f;
            entityManager.positions[id].z = (float)((i - 50) % 10) * 1.5f - 7.5f;
            entityManager.positions[id].y = 0.0f;
        }

        // Náhodná barva
        entityManager.colors[id].r = (float)rand() / RAND_MAX;
        entityManager.colors[id].g = (float)rand() / RAND_MAX;
        entityManager.colors[id].b = (float)rand() / RAND_MAX;

        // Rotace kolem osy X
        entityManager.rotations[id].angle = 0.0f;
        entityManager.rotations[id].axisX = 1.0f;
    }

    // 4. Inicializace Systémů
    TransformSystem transformSystem;
    RenderSystem renderSystem(shaderProgram, cubeVAO);

    float lastTime = 0.0f;

    // 5. Hlavní Smyčka
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // --- Zpracování Vstupů (Pohyb Kamery) ---
        processInput(window, deltaTime);

        // Clear
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Výpočet matic POHLEDU a PROJEKCE ---
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);


        // --- ECS Fáze 1: VÝPOČET ---
        transformSystem.update(deltaTime, entityManager);

        // --- ECS Fáze 2: VYKRESLENÍ ---
        renderSystem.update(entityManager, view, projection);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Čištění
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
#endif // ECS_H
