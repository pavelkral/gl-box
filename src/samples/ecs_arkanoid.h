// main.cpp - Arkanoid ECS (Entity Component System) v jednom souboru
// Vyžaduje: GLAD, GLFW, GLM
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <memory>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;



// --- I. KONSTANTY (C++11 constexpr) ---
namespace Constants {
// Window dimensions
constexpr unsigned int SCR_WIDTH = 1280;
constexpr unsigned int SCR_HEIGHT = 720;

// Gameplay geometry
constexpr float PADDLE_W = 200.0f;
constexpr float PADDLE_H = 20.0f;
constexpr float BALL_R = 8.0f; // radius

constexpr float PADDLE_START_Y = 50.0f;
constexpr float BALL_START_Y = 75.0f;

// Speeds
constexpr float PADDLE_SPEED = 800.0f; // units/sec
constexpr float BALL_INITIAL_SPEED_X = 200.0f;
constexpr float BALL_INITIAL_SPEED_Y = 300.0f;
constexpr float BALL_MIN_UPWARD_VELOCITY = 100.0f;
constexpr float BALL_SPIN_FACTOR = 150.0f;
constexpr float BALL_SPEEDUP_FACTOR = 1.02f;

// Scores & Lives
constexpr int REWARD_BRICK = 100;
constexpr int INITIAL_LIVES = 3;
}

// --- II. SHADERY (jednoduché, pro 2D) ---
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 model;
uniform mat4 projection;
void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)glsl";

// --- III. ECS KOMPONENTY ---
using EntityID = unsigned int;
struct PositionComponent { float x = 0, y = 0; };
struct VelocityComponent { float x = 0, y = 0; };
struct RenderComponent { float w = 1.0f, h = 1.0f; };
struct ColorComponent { float r = 1, g = 1, b = 1, a = 1; };

struct GameplayComponent {
    enum Type { NONE = 0, PADDLE = 1, BALL = 2, BRICK = 3 };
    Type type = NONE;
    int health = 1;
};

// --- IV. GLOBÁLNÍ STAV HRY A VSTUP ---
int score = 0;
int lives = Constants::INITIAL_LIVES;
bool leftPressed = false;
bool rightPressed = false;

// --- V. GLOBÁLNÍ OpenGL/GLM PROMĚNNÉ ---
GLuint shaderProgram;
GLuint quadVAO;
glm::mat4 projection;

// --- VI. ECS MANAGER ---
class EntityManager {
public:
    // Komponentové pole (Pole komponent: Array of Components)
    std::vector<PositionComponent> positions;
    std::vector<VelocityComponent> velocities;
    std::vector<RenderComponent> renders;
    std::vector<ColorComponent> colors;
    std::vector<GameplayComponent> gameplay;
    std::vector<bool> alive;

private:
    EntityID nextID = 0;

public:
    EntityID createEntity() {
        EntityID id = nextID++;
        // Zajištění synchronizace všech vektorů s novým ID
        if (id >= alive.size()) {
            positions.emplace_back();
            velocities.emplace_back();
            renders.emplace_back();
            colors.emplace_back();
            gameplay.emplace_back();
            alive.push_back(true);
        } else {
            // Znovupoužití ID
            alive[id] = true;
        }
        return id;
    }

    void destroyEntity(EntityID id) {
        if (id < alive.size()) alive[id] = false;
    }

    bool isAlive(EntityID id) const {
        return id < alive.size() && alive[id];
    }

    size_t size() const { return alive.size(); }

    EntityID findFirstOfType(GameplayComponent::Type type) const {
        for (size_t i = 0; i < alive.size(); ++i) {
            if (isAlive((EntityID)i) && gameplay[i].type == type) {
                return (EntityID)i;
            }
        }
        return (EntityID)-1;
    }
};

// ---  UTILITY A HELPERY ---

// AABB vs circle collision (přesunuto na globální funkci pro zjednodušení)
bool aabb_circle_collision(float ax, float ay, float aw, float ah,
                           float cx, float cy, float cr,
                           glm::vec2 &outNormal)
{
    float closestX = std::max(ax - aw / 2.0f, std::min(cx, ax + aw / 2.0f));
    float closestY = std::max(ay - ah / 2.0f, std::min(cy, ay + ah / 2.0f));

    float dx = cx - closestX;
    float dy = cy - closestY;
    float dist2 = dx * dx + dy * dy;

    if (dist2 > cr * cr) return false;

    glm::vec2 normal = glm::vec2(dx, dy);

    if (glm::length2(normal) < 1e-6f) {
        normal = glm::vec2(cx - ax, cy - ay);
    }

    outNormal = glm::normalize(normal);
    return true;
}

// OpenGL helper
void checkShaderCompileErrors(GLuint shader, const std::string& type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM::LINK_FAILED\n" << infoLog << std::endl;
        }
    }
}

GLuint createShaderProgram() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    checkShaderCompileErrors(vs, "VERTEX");

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    checkShaderCompileErrors(fs, "FRAGMENT");

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    checkShaderCompileErrors(prog, "PROGRAM");

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void initQuad() {
    float quadVertices[] = {
        // unit rectangle centered at origin
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f
    };
    glGenVertexArrays(1, &quadVAO);

    // *** DOPLNĚNÍ CHYBĚJÍCÍ DEKLARACE ***
    GLuint quadVBO;

    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

class System {
public:
    virtual void update(EntityManager& em, float deltaTime) = 0;
    virtual ~System() = default;
};

// 1) Paddle System
class PaddleSystem : public System {
public:
    void update(EntityManager& em, float deltaTime) override {
        for (size_t i = 0; i < em.size(); ++i) {
            if (!em.isAlive((EntityID)i) || em.gameplay[i].type != GameplayComponent::PADDLE) continue;

            float dir = 0.0f;
            if (leftPressed) dir -= 1.0f;
            if (rightPressed) dir += 1.0f;

            em.positions[i].x += dir * Constants::PADDLE_SPEED * deltaTime;

            // Omezení pohybu pádla
            float halfW = em.renders[i].w / 2.0f;
            float minX = halfW;
            float maxX = (float)Constants::SCR_WIDTH - halfW;

            em.positions[i].x = std::max(minX, std::min(em.positions[i].x, maxX));
        }
    }
};

// 2) Ball System
class BallSystem : public System {
private:
    void resetGame(EntityManager& em) {
        std::cout << "GAME OVER - Score: " << score << std::endl;
        lives = Constants::INITIAL_LIVES;
        score = 0;
        // Znovu-aktivace cihel
        for (size_t i = 0; i < em.size(); ++i) {
            if (em.gameplay[i].type == GameplayComponent::BRICK) {
                em.alive[i] = true;
            }
        }
    }

    void resetBallAndPaddle(EntityManager& em, EntityID ballId, EntityID paddleId) {
        // Reset pádla
        if (paddleId != (EntityID)-1) {
            em.positions[paddleId].x = (float)Constants::SCR_WIDTH * 0.5f;
            em.positions[paddleId].y = Constants::PADDLE_START_Y;
        }
        // Reset míčku
        em.positions[ballId].x = (float)Constants::SCR_WIDTH * 0.5f;
        em.positions[ballId].y = Constants::BALL_START_Y;
        em.velocities[ballId].x = Constants::BALL_INITIAL_SPEED_X * ((rand() % 2) ? 1.0f : -1.0f);
        em.velocities[ballId].y = Constants::BALL_INITIAL_SPEED_Y;
    }

public:
    void update(EntityManager& em, float deltaTime) override {
        EntityID ballId = em.findFirstOfType(GameplayComponent::BALL);
        EntityID paddleId = em.findFirstOfType(GameplayComponent::PADDLE);

        if (ballId == (EntityID)-1) return;

        PositionComponent& bp = em.positions[ballId];
        VelocityComponent& bv = em.velocities[ballId];
        float radius = em.renders[ballId].w * 0.5f;

        // 1. Pohyb
        bp.x += bv.x * deltaTime;
        bp.y += bv.y * deltaTime;

        // 2. Kolize se stěnami (horní, levá, pravá)
        if (bp.x - radius <= 0.0f) {
            bp.x = radius;
            bv.x = -bv.x;
        } else if (bp.x + radius >= (float)Constants::SCR_WIDTH) {
            bp.x = (float)Constants::SCR_WIDTH - radius;
            bv.x = -bv.x;
        }
        if (bp.y + radius >= (float)Constants::SCR_HEIGHT) {
            bp.y = (float)Constants::SCR_HEIGHT - radius;
            bv.y = -bv.y;
        }

        // Spodní: ztráta života
        if (bp.y - radius <= 0.0f) {
            lives--;
            std::cout << "Life lost! Remaining: " << lives << std::endl;

            if (lives <= 0) {
                resetGame(em);
            }
            resetBallAndPaddle(em, ballId, paddleId);
            return;
        }

        // 3. Kolize s pádlem
        if (paddleId != (EntityID)-1 && em.isAlive(paddleId)) {
            glm::vec2 normal;
            if (aabb_circle_collision(em.positions[paddleId].x, em.positions[paddleId].y,
                                      em.renders[paddleId].w, em.renders[paddleId].h,
                                      bp.x, bp.y, radius, normal))
            {
                glm::vec2 v = glm::vec2(bv.x, bv.y);
                glm::vec2 n = glm::normalize(normal);
                glm::vec2 r = v - 2.0f * glm::dot(v, n) * n;

                bv.x = r.x;
                bv.y = r.y;

                // Spin
                float rel = (bp.x - em.positions[paddleId].x) / (em.renders[paddleId].w * 0.5f);
                bv.x += rel * Constants::BALL_SPIN_FACTOR;

                // Zajištění pohybu nahoru
                if (bv.y < Constants::BALL_MIN_UPWARD_VELOCITY) {
                    bv.y = Constants::BALL_MIN_UPWARD_VELOCITY;
                }

                // Vytlačení míčku
                bp.y = em.positions[paddleId].y + em.renders[paddleId].h / 2.0f + radius + 0.5f;
            }
        }

        // 4. Kolize s cihlami
        for (size_t i = 0; i < em.size(); ++i) {
            if (!em.isAlive((EntityID)i) || em.gameplay[i].type != GameplayComponent::BRICK) continue;

            glm::vec2 normal;
            if (aabb_circle_collision(em.positions[i].x, em.positions[i].y,
                                      em.renders[i].w, em.renders[i].h,
                                      bp.x, bp.y, radius, normal))
            {
                // Zničení cihly
                em.destroyEntity((EntityID)i);
                score += Constants::REWARD_BRICK;

                // Odraz míčku
                glm::vec2 v = glm::vec2(bv.x, bv.y);
                glm::vec2 n = glm::normalize(normal);
                glm::vec2 r = v - 2.0f * glm::dot(v, n) * n;
                bv.x = r.x;
                bv.y = r.y;

                // Mírné zrychlení
                bv.x *= Constants::BALL_SPEEDUP_FACTOR;
                bv.y *= Constants::BALL_SPEEDUP_FACTOR;

                break; // Pouze jedna cihla za snímek
            }
        }
    }
};

// 3) Render System
class RenderSystem : public System {
public:
    void update(EntityManager& em, float deltaTime) override {
        glUseProgram(shaderProgram);
        glBindVertexArray(quadVAO);

        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
        GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        for (size_t i = 0; i < em.size(); ++i) {
            if (!em.isAlive((EntityID)i)) continue;

            // Model matice: Transformace z jednotkového čtverce na herní pozici/velikost
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(em.positions[i].x, em.positions[i].y, 0.0f));
            model = glm::scale(model, glm::vec3(em.renders[i].w, em.renders[i].h, 1.0f));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform4f(colorLoc, em.colors[i].r, em.colors[i].g, em.colors[i].b, em.colors[i].a);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }
};

// --- IX. GAME SETUP A MAIN ---

// Input callback
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) {
        leftPressed = action != GLFW_RELEASE;
    }
    if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) {
        rightPressed = action != GLFW_RELEASE;
    }
}

// Helper: create bricks grid
void createBricksGrid(EntityManager& em, int cols, int rows, float startX, float startY, float brickW, float brickH, float padding) {
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            EntityID id = em.createEntity();
            em.gameplay[id].type = GameplayComponent::BRICK;
            em.positions[id].x = startX + c * (brickW + padding);
            em.positions[id].y = startY - r * (brickH + padding);
            em.renders[id].w = brickW;
            em.renders[id].h = brickH;

            // Náhodná barva
            em.colors[id].r = 0.3f + 0.7f * ((float)rand() / RAND_MAX);
            em.colors[id].g = 0.3f + 0.7f * ((float)rand() / RAND_MAX);
            em.colors[id].b = 0.3f + 0.7f * ((float)rand() / RAND_MAX);
        }
    }
}

// Setup initial game entities
void setupGame(EntityManager& em) {
    // paddle
    EntityID paddle = em.createEntity();
    em.gameplay[paddle].type = GameplayComponent::PADDLE;
    em.renders[paddle].w = Constants::PADDLE_W;
    em.renders[paddle].h = Constants::PADDLE_H;
    em.positions[paddle].x = (float)Constants::SCR_WIDTH * 0.5f;
    em.positions[paddle].y = Constants::PADDLE_START_Y;
    em.colors[paddle] = { 0.8f, 0.8f, 0.2f, 1.0f };

    // ball
    EntityID ball = em.createEntity();
    em.gameplay[ball].type = GameplayComponent::BALL;
    em.renders[ball].w = Constants::BALL_R * 2.0f;
    em.renders[ball].h = Constants::BALL_R * 2.0f;
    em.positions[ball].x = (float)Constants::SCR_WIDTH * 0.5f;
    em.positions[ball].y = Constants::BALL_START_Y;
    em.velocities[ball].x = Constants::BALL_INITIAL_SPEED_X * ((rand() % 2) ? 1.0f : -1.0f);
    em.velocities[ball].y = Constants::BALL_INITIAL_SPEED_Y;
    em.colors[ball] = { 1.0f, 0.5f, 0.2f, 1.0f };

    // bricks grid setup
    int cols = 10;
    int rows = 6;
    float brickW = 100.0f;
    float brickH = 30.0f;
    float pad = 8.0f;
    float totalW = cols * brickW + (cols - 1) * pad;
    float startX = ((float)Constants::SCR_WIDTH - totalW) * 0.5f + brickW * 0.5f;
    float startY = (float)Constants::SCR_HEIGHT - 120.0f;
    createBricksGrid(em, cols, rows, startX, startY, brickW, brickH, pad);
}

// Main
int main() {
    srand((unsigned int)time(nullptr));

    // 1. GLFW init
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(Constants::SCR_WIDTH, Constants::SCR_HEIGHT, "Arkanoid - Single File ECS OpenGL", NULL, NULL);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to init GLAD\n"; return -1; }
    glViewport(0, 0, Constants::SCR_WIDTH, Constants::SCR_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 2. Init OpenGL assets
    shaderProgram = createShaderProgram();
    initQuad();

    // Orthographic projection
    projection = glm::ortho(0.0f, (float)Constants::SCR_WIDTH, 0.0f, (float)Constants::SCR_HEIGHT, -1.0f, 1.0f);

    // 3. Init ECS and Systems
    EntityManager em;
    setupGame(em);

    std::vector<std::unique_ptr<System>> systems;
    systems.push_back(std::make_unique<PaddleSystem>());
    systems.push_back(std::make_unique<BallSystem>());
    systems.push_back(std::make_unique<RenderSystem>());

    float lastTime = (float)glfwGetTime();

    // 4. Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        if (dt > 0.033f) dt = 0.033f;
        lastTime = currentTime;

        glfwPollEvents();

        // Game Update (Systems)
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Spuštění logických/fyzikálních systémů
        for (const auto& system : systems) {
            if (dynamic_cast<RenderSystem*>(system.get()) == nullptr) {
                system->update(em, dt);
            }
        }

        // Spuštění Render Systemu
        for (const auto& system : systems) {
            if (dynamic_cast<RenderSystem*>(system.get()) != nullptr) {
                system->update(em, dt);
            }
        }

        // HUD print
        static float hudTimer = 0.0f;
        hudTimer += dt;
        if (hudTimer > 1.0f) {
            hudTimer = 0.0f;
            std::cout << "Score: " << score << "  Lives: " << lives << std::endl;
        }

        glfwSwapBuffers(window);
    }

    // 5. Cleanup
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
