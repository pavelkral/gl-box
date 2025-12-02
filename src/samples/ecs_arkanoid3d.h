#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <random>
#include <memory>
#include <array>
#include <optional>
#include <unordered_map>
#include <set>

// -------------------- Config (Zachováno) --------------------
namespace Config {
namespace Camera{
constexpr unsigned int SCREEN_WIDTH  = 1280;
constexpr unsigned int SCREEN_HEIGHT = 720;
inline constexpr glm::vec3 CAMERA_POS    = {0.0f, 8.0f, 95.0f};
inline constexpr glm::vec3 CAMERA_FRONT = {0.0f, -0.15f, -1.0f};
inline constexpr glm::vec3 CAMERA_UP     = {0.0f, 1.0f, 0.0f};
}
namespace World {
constexpr float MIN_X = -60.0f;
constexpr float MAX_X = 60.0f;
constexpr float MIN_Y = -40.0f;
constexpr float MAX_Y = 20.0f;
}
namespace Bricks {
constexpr int ROWS = 10;
constexpr int COLS = 30;
constexpr float SPACING_X = 3.0f;
constexpr float SPACING_Y = 2.0f;
constexpr float START_X = -13.5f;
constexpr float START_Y = 2.0f;
inline constexpr glm::vec3 SCALE = {2.5f, 1.8f, 2.0f};
}
namespace Paddle {
inline constexpr glm::vec3 START_POS = {0.0f, -30.0f, 0.0f};
inline constexpr glm::vec3 SCALE = {10.0f, 2.0f, 2.0f};
}
namespace Ball {
inline constexpr glm::vec3 START_POS = {0.0f, -25.0f, 0.0f};
inline constexpr glm::vec3 START_VEL = {10.0f, 16.0f, 0.0f};
constexpr float RADIUS = 1.0f;
constexpr float SPEEDUP_FACTOR = 1.15f;
constexpr float MAX_SPEED = 40.0f;
}
namespace Stats {
constexpr int INITIAL_LIVES = 3;
constexpr int SCORE_PER_BRICK = 10;
}
}

// -------------------- Utilities (Zachováno) --------------------
struct Stats {
    float deltaTime = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    float fps = 0.0f;

    void update(float dt) {
        deltaTime = dt;
        frameCount++;
        fpsTimer += dt;
        if(fpsTimer >= 1.0f){
            fps = (float)frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }
    void drawUI(){
        ImGui::SetNextWindowPos(ImVec2(1280 - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("Stats", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);

        // Zvětšení textu (volitelné)
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
        ImGui::End();
    }
};

class Random {
public:
    static float Float(float min, float max) {
        static std::mt19937 mt{std::random_device{}()};
        std::uniform_real_distribution<float> dist(min, max);
        return dist(mt);
    }
    static glm::vec4 RandomColor() {
        return glm::vec4(Float(0.2f, 1.0f), Float(0.2f, 1.0f), Float(0.2f, 1.0f), 1.0f);
    }
};

// -------------------- OpenGL Wrappers (Zachováno) --------------------
class Shader {
public:
    GLuint ID = 0;
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        ID = glCreateProgram();
        glAttachShader(ID, vs);
        glAttachShader(ID, fs);
        glLinkProgram(ID);
        checkLinkErrors(ID);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
    ~Shader() { if (ID) glDeleteProgram(ID); }
    Shader(const Shader&) = delete;
    void use() const { glUseProgram(ID); }
private:
    GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        checkCompileErrors(shader, type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
        return shader;
    }
    void checkCompileErrors(GLuint shader, std::string type) {
        GLint success; char infoLog[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) { glGetShaderInfoLog(shader, 1024, NULL, infoLog); std::cerr << "SHADER::" << type << "\n" << infoLog << std::endl; }
    }
    void checkLinkErrors(GLuint program) {
        GLint success; char infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) { glGetProgramInfoLog(program, 1024, NULL, infoLog); std::cerr << "LINK::ERROR\n" << infoLog << std::endl; }
    }
};

class Buffer {
public:
    GLuint ID = 0;
    GLenum type;
    Buffer(GLenum type) : type(type) { glGenBuffers(1, &ID); }
    ~Buffer() { if(ID) glDeleteBuffers(1, &ID); }
    void bind() const { glBindBuffer(type, ID); }
    void unbind() const { glBindBuffer(type, 0); }
    template<typename T> void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
        bind(); glBufferData(type, data.size() * sizeof(T), data.data(), usage);
    }
    void allocate(size_t size, GLenum usage = GL_DYNAMIC_DRAW) {
        bind(); glBufferData(type, size, nullptr, usage);
    }
    template<typename T> void setSubData(const std::vector<T>& data, size_t offset = 0) {
        bind(); glBufferSubData(type, offset, data.size() * sizeof(T), data.data());
    }
    template<typename T> void setSubDataSingle(const T& data, size_t offset = 0) {
        bind(); glBufferSubData(type, offset, sizeof(T), &data);
    }
};

class VertexArray {
public:
    GLuint ID = 0;
    VertexArray() { glGenVertexArrays(1, &ID); }
    ~VertexArray() { if(ID) glDeleteVertexArrays(1, &ID); }
    void bind() const { glBindVertexArray(ID); }
    void unbind() const { glBindVertexArray(0); }
};

struct Mesh {
    std::unique_ptr<VertexArray> vao;
    std::unique_ptr<Buffer> vbo;
    std::unique_ptr<Buffer> ebo;
    GLsizei indexCount = 0;

    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
        vao = std::make_unique<VertexArray>();
        vbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        ebo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);
        vao->bind();
        vbo->setData(vertices);
        ebo->setData(indices);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        vao->unbind();
        indexCount = (GLsizei)indices.size();
    }
    void drawInstanced(GLsizei count) const {
        vao->bind();
        glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, count);
        vao->unbind();
    }
};

// Mesh Helpers
namespace MeshFactory {
std::unique_ptr<Mesh> createCube() {
    std::vector<float> vertices = {
        -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    std::vector<unsigned int> indices = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 0,1,5, 5,4,0, 2,3,7, 7,6,2, 0,3,7, 7,4,0, 1,2,6, 6,5,1
    };
    return std::make_unique<Mesh>(vertices, indices);
}
std::unique_ptr<Mesh> createSphere(float radius, int latSeg = 16, int longSeg = 16) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    for (int y=0;y<=latSeg;y++){
        float theta = (float)y * glm::pi<float>() / latSeg;
        for (int x=0;x<=longSeg;x++){
            float phi = (float)x * 2.0f * glm::pi<float>() / longSeg;
            vertices.push_back(radius * cos(phi) * sin(theta));
            vertices.push_back(radius * cos(theta));
            vertices.push_back(radius * sin(phi) * sin(theta));
        }
    }
    for (int y=0;y<latSeg;y++){
        for (int x=0;x<longSeg;x++){
            int a = y*(longSeg+1)+x;
            int b = a+longSeg+1;
            indices.push_back(a); indices.push_back(b); indices.push_back(a+1);
            indices.push_back(b); indices.push_back(b+1); indices.push_back(a+1);
        }
    }
    return std::make_unique<Mesh>(vertices, indices);
}
}

// -------------------- Shaders Code (Zachováno) --------------------
const char* VS_SRC = R"glsl(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aRow0;
layout(location = 2) in vec4 aRow1;
layout(location = 3) in vec4 aRow2;
layout(location = 4) in vec4 aRow3;
layout(location = 5) in vec4 aColor;
layout(std140, binding = 0) uniform Camera { mat4 view; mat4 projection; };
out vec3 vPos;
out vec3 vNormal;
out vec4 vColor;
void main() {
    mat4 model = mat4(aRow0, aRow1, aRow2, aRow3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vPos = worldPos.xyz;
    vNormal = normalize(mat3(model) * aPos);
    vColor = aColor;
    gl_Position = projection * view * worldPos;
}
)glsl";

const char* FS_SRC = R"glsl(
#version 450 core
in vec3 vPos;
in vec3 vNormal;
in vec4 vColor;
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(10.0, 20.0, 10.0) - vPos);
    vec3 V = normalize(vec3(0.0, 15.0, 35.0) - vPos);
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 base = vColor.rgb * 0.6 + vColor.rgb * 0.4 * diff;
    vec3 color = base + vec3(1.0) * 0.6 * spec;
    FragColor = vec4(color, vColor.a);
}
)glsl";

// -------------------- ECS: Components --------------------
// Komponenty jsou jen data, žádné metody

using Entity = std::uint32_t;
const Entity NULL_ENTITY = 0;

// Identifikace typu entity
enum class TagType { None, Paddle, Ball, Brick };

struct TagComponent {
    TagType type = TagType::None;
};

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};

    // Helper pro získání matice (použito render systémem)
    glm::mat4 getMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::scale(m, scale);
        return m;
    }
};

struct RigidbodyComponent {
    glm::vec3 velocity{0.0f};
    bool isStatic = false; // Pokud true, fyzika s tím nehýbe (bricks)
};

struct ColliderComponent {
    enum Type { Box, Sphere } type;
    float radius = 1.0f; // Jen pro sféru
    // Box bere rozměry z Transform.scale
};

struct RenderComponent {
    Mesh* mesh = nullptr; // Raw pointer (resources vlastní Game třída)
    glm::vec4 color{1.0f};
    bool visible = true;
};

struct PlayerControlComponent {
    float lastX = 0.0f;     // Pro výpočet "faleše"
    float velocityX = 0.0f; // Vypočtená rychlost pádla
};

struct GameStateComponent {
    // Komponenta pro Ball entity, říká jestli je ve hře
    bool launched = false;
};

// -------------------- ECS: Registry --------------------
// Jednoduchý správce entit a komponent (Concrete ECS implementation)

class Registry {
public:
    std::vector<Entity> entities;
    std::uint32_t nextId = 1;

    // Komponenty uložené jako pole indexované [Entity ID]
    // Používáme mapu pro jednoduchost v tomto příkladu, v produkčním ECS by to byl sparse-set
    std::unordered_map<Entity, TagComponent> tags;
    std::unordered_map<Entity, TransformComponent> transforms;
    std::unordered_map<Entity, RigidbodyComponent> rigidbodies;
    std::unordered_map<Entity, ColliderComponent> colliders;
    std::unordered_map<Entity, RenderComponent> renderables;
    std::unordered_map<Entity, PlayerControlComponent> players;
    std::unordered_map<Entity, GameStateComponent> gameStates; // Např. na míčku

    // Globální stav hry (Resource)
    struct {
        int score = 0;
        int lives = Config::Stats::INITIAL_LIVES;
        bool gameOver = false;
        bool gameWon = false;
    } globalState;

    Entity createEntity() {
        Entity id = nextId++;
        entities.push_back(id);
        return id;
    }

    void destroyEntity(Entity e) {
        entities.erase(std::remove(entities.begin(), entities.end(), e), entities.end());
        tags.erase(e);
        transforms.erase(e);
        rigidbodies.erase(e);
        colliders.erase(e);
        renderables.erase(e);
        players.erase(e);
        gameStates.erase(e);
    }

    void clear() {
        entities.clear();
        tags.clear();
        transforms.clear();
        rigidbodies.clear();
        colliders.clear();
        renderables.clear();
        players.clear();
        gameStates.clear();
        nextId = 1;
    }

    // Utility helpers
    template<typename T> T& addComponent(Entity e, T component);
    template<typename T> T* getComponent(Entity e);
    template<typename T> bool hasComponent(Entity e);
};

// Template specializace pro zjednodušení syntaxe
template<> TagComponent& Registry::addComponent(Entity e, TagComponent c) { return tags[e] = c; }
template<> TransformComponent& Registry::addComponent(Entity e, TransformComponent c) { return transforms[e] = c; }
template<> RigidbodyComponent& Registry::addComponent(Entity e, RigidbodyComponent c) { return rigidbodies[e] = c; }
template<> ColliderComponent& Registry::addComponent(Entity e, ColliderComponent c) { return colliders[e] = c; }
template<> RenderComponent& Registry::addComponent(Entity e, RenderComponent c) { return renderables[e] = c; }
template<> PlayerControlComponent& Registry::addComponent(Entity e, PlayerControlComponent c) { return players[e] = c; }
template<> GameStateComponent& Registry::addComponent(Entity e, GameStateComponent c) { return gameStates[e] = c; }

template<> TagComponent* Registry::getComponent(Entity e) { return tags.count(e) ? &tags[e] : nullptr; }
template<> TransformComponent* Registry::getComponent(Entity e) { return transforms.count(e) ? &transforms[e] : nullptr; }
template<> RigidbodyComponent* Registry::getComponent(Entity e) { return rigidbodies.count(e) ? &rigidbodies[e] : nullptr; }
template<> ColliderComponent* Registry::getComponent(Entity e) { return colliders.count(e) ? &colliders[e] : nullptr; }
template<> RenderComponent* Registry::getComponent(Entity e) { return renderables.count(e) ? &renderables[e] : nullptr; }
template<> PlayerControlComponent* Registry::getComponent(Entity e) { return players.count(e) ? &players[e] : nullptr; }
template<> GameStateComponent* Registry::getComponent(Entity e) { return gameStates.count(e) ? &gameStates[e] : nullptr; }

// -------------------- ECS: Systems --------------------

class InputSystem {
public:
    void Update(Registry& registry, GLFWwindow* window, float dt) {
        // Hledáme hráče (Paddle)
        for (auto& [entity, playerCtrl] : registry.players) {
            auto* transform = registry.getComponent<TransformComponent>(entity);
            if (!transform) continue;

            // Mouse logic
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            float normX = (float)xpos / width;
            float worldX = Config::World::MIN_X + normX * (Config::World::MAX_X - Config::World::MIN_X);

            float prevX = transform->position.x;
            // Smooth movement
            transform->position.x += (worldX - transform->position.x) * 15.0f * dt;

            // Clamp
            float halfW = transform->scale.x * 0.5f;
            transform->position.x = std::clamp(transform->position.x, Config::World::MIN_X + halfW, Config::World::MAX_X - halfW);

            // Výpočet rychlosti pádla pro faleš
            if (dt > 0.0f) {
                playerCtrl.velocityX = (transform->position.x - prevX) / dt;
            }
            playerCtrl.lastX = transform->position.x;
        }

        // Space to launch
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            for (auto& [entity, state] : registry.gameStates) {
                if (registry.tags[entity].type == TagType::Ball && !state.launched && !registry.globalState.gameOver) {
                    state.launched = true;
                    // Reset velocity vector if needed, or rely on initialization
                }
            }
        }
    }
};

class PhysicsSystem {
public:
    void Update(Registry& registry, float dt) {
        if (registry.globalState.gameOver) return;

        // 1. Move Ball
        Entity ballEntity = NULL_ENTITY;
        TransformComponent* ballTrans = nullptr;
        RigidbodyComponent* ballRb = nullptr;
        GameStateComponent* ballState = nullptr;
        ColliderComponent* ballCol = nullptr;

        // Najdeme míček (v ECS bychom ideálně iterovali přes View<Tag, Transform...>)
        for (auto& [e, tag] : registry.tags) {
            if (tag.type == TagType::Ball) {
                ballEntity = e;
                ballTrans = registry.getComponent<TransformComponent>(e);
                ballRb = registry.getComponent<RigidbodyComponent>(e);
                ballState = registry.getComponent<GameStateComponent>(e);
                ballCol = registry.getComponent<ColliderComponent>(e);
                break;
            }
        }

        if (!ballEntity || !ballState) return;

        // Logic for unlaunched ball (sticky to paddle)
        if (!ballState->launched) {
            // Find paddle
            for (auto& [pe, ptag] : registry.tags) {
                if (ptag.type == TagType::Paddle) {
                    auto* pTrans = registry.getComponent<TransformComponent>(pe);
                    if (pTrans) {
                        ballTrans->position.x = pTrans->position.x;
                        ballTrans->position.y = pTrans->position.y + pTrans->scale.y * 0.5f + ballCol->radius + 0.2f;
                    }
                    break;
                }
            }
            return;
        }

        // Physics Integration
        ballTrans->position += ballRb->velocity * dt;

        // Wall collisions
        if (ballTrans->position.x <= Config::World::MIN_X) {
            ballTrans->position.x = Config::World::MIN_X;
            ballRb->velocity.x *= -1.0f;
        } else if (ballTrans->position.x >= Config::World::MAX_X) {
            ballTrans->position.x = Config::World::MAX_X;
            ballRb->velocity.x *= -1.0f;
        }
        if (ballTrans->position.y >= Config::World::MAX_Y) {
            ballTrans->position.y = Config::World::MAX_Y;
            ballRb->velocity.y *= -1.0f;
        }

        // Entity Collisions
        // Iterate all entities with Colliders that are NOT the ball
        std::vector<Entity> destroyedEntities;

        for (auto& [targetE, targetCol] : registry.colliders) {
            if (targetE == ballEntity) continue;

            auto* targetTrans = registry.getComponent<TransformComponent>(targetE);
            if (!targetTrans) continue;

            if (checkAABB(targetTrans->position, targetTrans->scale, ballTrans->position, ballCol->radius)) {

                TagComponent* tag = registry.getComponent<TagComponent>(targetE);

                // Paddle Collision
                if (tag && tag->type == TagType::Paddle) {
                    glm::vec3 normal(0.0f, 1.0f, 0.0f);
                    ballRb->velocity = reflectVector(ballRb->velocity, normal);

                    // Add spin from player component
                    auto* playerCtrl = registry.getComponent<PlayerControlComponent>(targetE);
                    if (playerCtrl) {
                        ballRb->velocity.x += playerCtrl->velocityX * 0.12f;
                    }

                    // Push out
                    ballTrans->position.y = targetTrans->position.y + targetTrans->scale.y * 0.5f + ballCol->radius + 0.1f;

                    // Speed up
                    applySpeedup(ballRb->velocity);
                }
                // Brick Collision
                else if (tag && tag->type == TagType::Brick) {
                    destroyedEntities.push_back(targetE);
                    registry.globalState.score += Config::Stats::SCORE_PER_BRICK;

                    // Resolve Bounce
                    glm::vec3 delta = ballTrans->position - targetTrans->position;
                    glm::vec3 normal;
                    // Velmi hrubá aproximace normály
                    if (std::abs(delta.x) > std::abs(delta.y)) { // Více ze strany
                        normal = glm::vec3(delta.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
                    } else { // Více z vrchu/spodu
                        normal = glm::vec3(0.0f, delta.y > 0 ? 1.0f : -1.0f, 0.0f);
                    }
                    ballRb->velocity = reflectVector(ballRb->velocity, normal);
                    applySpeedup(ballRb->velocity);
                }
            }
        }

        // Remove destroyed bricks
        for (auto e : destroyedEntities) {
            registry.destroyEntity(e);
        }
    }

private:
    bool checkAABB(const glm::vec3& bPos, const glm::vec3& bScale, const glm::vec3& spherePos, float r) {
        float halfW = bScale.x * 0.5f;
        float halfH = bScale.y * 0.5f;
        return (spherePos.x + r > bPos.x - halfW && spherePos.x - r < bPos.x + halfW &&
                spherePos.y + r > bPos.y - halfH && spherePos.y - r < bPos.y + halfH);
    }

    glm::vec3 reflectVector(const glm::vec3& v, const glm::vec3& normal) {
        return v - 2.0f * glm::dot(v, normal) * normal;
    }

    void applySpeedup(glm::vec3& vel) {
        vel *= Config::Ball::SPEEDUP_FACTOR;
        if (glm::length(vel) > Config::Ball::MAX_SPEED)
            vel = glm::normalize(vel) * Config::Ball::MAX_SPEED;
    }
};

class GameLogicSystem {
public:
    void Update(Registry& registry) {
        // Check Death
        Entity ballEntity = NULL_ENTITY;
        for (auto& [e, tag] : registry.tags) if (tag.type == TagType::Ball) ballEntity = e;

        if (ballEntity != NULL_ENTITY) {
            auto* trans = registry.getComponent<TransformComponent>(ballEntity);
            if (trans && trans->position.y < Config::World::MIN_Y) {
                registry.globalState.lives--;
                if (registry.globalState.lives <= 0) {
                    registry.globalState.gameOver = true;
                } else {
                    // Reset Ball & Paddle
                    resetRound(registry, ballEntity);
                }
            }
        }

        // Check Victory
        bool anyBrick = false;
        for (auto& [e, tag] : registry.tags) {
            if (tag.type == TagType::Brick) {
                anyBrick = true;
                break;
            }
        }
        if (!anyBrick) {
            registry.globalState.gameWon = true;
            registry.globalState.gameOver = true;
        }
    }

    void resetRound(Registry& registry, Entity ball) {
        auto* ballState = registry.getComponent<GameStateComponent>(ball);
        auto* ballRb = registry.getComponent<RigidbodyComponent>(ball);
        auto* ballTrans = registry.getComponent<TransformComponent>(ball);

        if (ballState) ballState->launched = false;
        if (ballRb) ballRb->velocity = Config::Ball::START_VEL;
        // Position se resetuje v PhysicsSystem (sticky logic)

        // Reset Paddle Position
        for(auto& [e, tag] : registry.tags) {
            if(tag.type == TagType::Paddle) {
                auto* pt = registry.getComponent<TransformComponent>(e);
                if(pt) pt->position = Config::Paddle::START_POS;
                auto* pc = registry.getComponent<PlayerControlComponent>(e);
                if(pc) { pc->velocityX = 0; pc->lastX = Config::Paddle::START_POS.x; }
            }
        }
    }
};

class RenderSystem {
    // Buffery pro instancing
    std::unique_ptr<Buffer> vboInstance;
    std::unique_ptr<Buffer> vboColor;
    // Temporary vectors
    std::vector<glm::mat4> matrices;
    std::vector<glm::vec4> colors;

public:
    void Init() {
        // Allocate max size
        size_t maxInstances = Config::Bricks::ROWS * Config::Bricks::COLS + 100;
        vboInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboInstance->allocate(maxInstances * sizeof(glm::mat4));
        vboColor = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboColor->allocate(maxInstances * sizeof(glm::vec4));
    }

    // Nastaví VAO pro instancování (musí se volat pro každý Mesh, který to podporuje)
    void SetupVAO(const VertexArray& vao) {
        vao.bind();
        vboInstance->bind();
        size_t vec4Size = sizeof(glm::vec4);
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
            glVertexAttribDivisor(1 + i, 1);
        }
        vboColor->bind();
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
        glVertexAttribDivisor(5, 1);
        vao.unbind();
    }

    void Update(Registry& registry, Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
        shader.use();
        // Camera Uniforms jsou v UBO spravovaném v Game třídě, zde jen kreslíme

        // Seskupíme entity podle Meshe pro efektivní instancing
        // Mesh* -> list of entities
        std::unordered_map<Mesh*, std::vector<Entity>> batch;

        for (auto& [e, rend] : registry.renderables) {
            if (!rend.visible || !rend.mesh) continue;
            batch[rend.mesh].push_back(e);
        }

        // Render Batches
        for (auto& [mesh, entities] : batch) {
            matrices.clear();
            colors.clear();
            matrices.reserve(entities.size());
            colors.reserve(entities.size());

            for (Entity e : entities) {
                auto* trans = registry.getComponent<TransformComponent>(e);
                auto* rend = registry.getComponent<RenderComponent>(e);
                if (trans && rend) {
                    matrices.push_back(trans->getMatrix());
                    colors.push_back(rend->color);
                }
            }

            if (!matrices.empty()) {
                vboInstance->setSubData(matrices);
                vboColor->setSubData(colors);
                mesh->drawInstanced((GLsizei)matrices.size());
            }
        }
    }

    void DrawUI(Registry& registry, Stats& stats) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Vykreslit Stats UI (Teď je to bezpečné, jsme uvnitř framu)
        stats.drawUI();

        // 2. Vykreslit Game UI
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("GameInfo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Score: %d", registry.globalState.score);
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Lives: %d", registry.globalState.lives);
        ImGui::End();

        if (registry.globalState.gameOver) {
            if(!registry.globalState.gameWon) ImGui::OpenPopup("GameOver");
            else ImGui::OpenPopup("GameWon");
        }

        if (ImGui::BeginPopupModal("GameOver", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("GAME OVER");
            ImGui::Text("Final Score: %d", registry.globalState.score);
            // Pro restart implementuj callback nebo check v Game loopu
            ImGui::Text("(Press R to Restart)");
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("GameWon", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("CONGRATULATIONS!");
            ImGui::Text("Score: %d", registry.globalState.score);
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
};

// -------------------- Game Class (Orchestrator) --------------------
class Game {
    struct WindowDeleter { void operator()(GLFWwindow* ptr) { glfwDestroyWindow(ptr); } };
    std::unique_ptr<GLFWwindow, WindowDeleter> window;

    // Resources
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<Mesh> sphereMesh;
    std::unique_ptr<Buffer> uboCamera;

    // ECS
    Registry registry;
    InputSystem inputSystem;
    PhysicsSystem physicsSystem;
    RenderSystem renderSystem;
    GameLogicSystem logicSystem;

    Stats stats;
    glm::mat4 view, proj;

public:
    bool init() {
        if (!glfwInit()) return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window.reset(glfwCreateWindow(Config::Camera::SCREEN_WIDTH, Config::Camera::SCREEN_HEIGHT, "Arkanoid ECS", nullptr, nullptr));
        if (!window) return false;
        glfwMakeContextCurrent(window.get());
        glfwSwapInterval(0);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

        glEnable(GL_DEPTH_TEST);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
        ImGui_ImplOpenGL3_Init("#version 450");

        initResources();
        resetGame();
        return true;
    }

    void run() {
        view = glm::lookAt(Config::Camera::CAMERA_POS, Config::Camera::CAMERA_POS + Config::Camera::CAMERA_FRONT, Config::Camera::CAMERA_UP);
        proj = glm::perspective(glm::radians(45.0f), (float)Config::Camera::SCREEN_WIDTH / Config::Camera::SCREEN_HEIGHT, 0.1f, 100.0f);

        float lastTime = (float)glfwGetTime();
        while (!glfwWindowShouldClose(window.get())) {
            float now = (float)glfwGetTime();
            float dt = std::min(now - lastTime, 0.05f);
            lastTime = now;

            glfwPollEvents();

            // --- ECS Update Loop ---

            // 1. Input
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window.get(), true);
            inputSystem.Update(registry, window.get(), dt);

            // 2. Logic & Physics
            if (!registry.globalState.gameOver) {
                physicsSystem.Update(registry, dt);
                logicSystem.Update(registry);
            } else {
                // Restart logic (simple hack)
                if (glfwGetKey(window.get(), GLFW_KEY_R) == GLFW_PRESS) {
                    resetGame();
                }
            }

            // 3. Render
            // Update Camera UBO
            uboCamera->bind();
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));
            uboCamera->unbind();

            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderSystem.Update(registry, *shader, view, proj);

            stats.update(dt);

            // --- OPRAVA ZDE ---
            // Smazáno: stats.drawUI();  <-- TOTO ZPŮSOBOVALO CHYBU
            // Upraveno: Posíláme stats dovnitř
            renderSystem.DrawUI(registry, stats);

            glfwSwapBuffers(window.get());
        }
        cleanup();
    }

private:
    void initResources() {
        shader = std::make_unique<Shader>(VS_SRC, FS_SRC);
        cubeMesh = MeshFactory::createCube();
        sphereMesh = MeshFactory::createSphere(Config::Ball::RADIUS);

        uboCamera = std::make_unique<Buffer>(GL_UNIFORM_BUFFER);
        uboCamera->allocate(2 * sizeof(glm::mat4));
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboCamera->ID);

        renderSystem.Init();
        renderSystem.SetupVAO(*cubeMesh->vao);
        renderSystem.SetupVAO(*sphereMesh->vao);
    }

    void resetGame() {
        registry.clear();
        registry.globalState = {0, Config::Stats::INITIAL_LIVES, false, false};

        // --- Create Entities ---

        // 1. Paddle
        Entity paddle = registry.createEntity();
        registry.addComponent(paddle, TagComponent{TagType::Paddle});
        registry.addComponent(paddle, TransformComponent{Config::Paddle::START_POS, Config::Paddle::SCALE});
        registry.addComponent(paddle, RenderComponent{cubeMesh.get(), glm::vec4(0.3f, 0.8f, 0.3f, 1.0f)});
        registry.addComponent(paddle, ColliderComponent{ColliderComponent::Box});
        registry.addComponent(paddle, PlayerControlComponent{});

        // 2. Ball
        Entity ball = registry.createEntity();
        registry.addComponent(ball, TagComponent{TagType::Ball});
        registry.addComponent(ball, TransformComponent{Config::Ball::START_POS, glm::vec3(Config::Ball::RADIUS)}); // Scale only visual
        registry.addComponent(ball, RigidbodyComponent{Config::Ball::START_VEL});
        registry.addComponent(ball, RenderComponent{sphereMesh.get(), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f)});
        registry.addComponent(ball, ColliderComponent{ColliderComponent::Sphere, Config::Ball::RADIUS});
        registry.addComponent(ball, GameStateComponent{false});

        // 3. Bricks
        int rows = Config::Bricks::ROWS;
        int cols = Config::Bricks::COLS;
        float totalWidth = Config::World::MAX_X - Config::World::MIN_X;
        float spacingX = 0.2f;
        float brickWidth = (totalWidth - (cols - 1) * spacingX) / cols;
        float brickHeight = Config::Bricks::SCALE.y;
        float startX = Config::World::MIN_X + brickWidth * 0.5f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Entity brick = registry.createEntity();
                glm::vec3 pos = {
                    startX + c * (brickWidth + spacingX),
                    Config::Bricks::START_Y + r * (brickHeight + spacingX),
                    0.0f
                };
                glm::vec3 scale = {brickWidth, brickHeight, Config::Bricks::SCALE.z};

                registry.addComponent(brick, TagComponent{TagType::Brick});
                registry.addComponent(brick, TransformComponent{pos, scale});
                registry.addComponent(brick, RenderComponent{cubeMesh.get(), Random::RandomColor()});
                registry.addComponent(brick, ColliderComponent{ColliderComponent::Box});
            }
        }
    }

    void cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

int main() {
    Game game;
    if (game.init()) {
        game.run();
    }
    return 0;
}
