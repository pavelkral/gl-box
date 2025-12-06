#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <functional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <random>
#include <memory>
#include <array>
#include <optional>
#include <unordered_map>

// -------------------- Config --------------------
namespace Config {
    namespace Camera{
    constexpr unsigned int SCREEN_WIDTH  = 1920;
    constexpr unsigned int SCREEN_HEIGHT = 1080;
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
    constexpr int COLS = 10;
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
    constexpr float MAX_SPEED = 45.0f;
    }
    namespace Stats {
    constexpr int INITIAL_LIVES = 3;
    constexpr int SCORE_PER_BRICK = 10;
    }
    namespace PowerUp {
    constexpr float FALL_SPEED = 15.0f;
    constexpr float DROP_CHANCE = 0.20f; // 20% šance
    constexpr float DURATION = 10.0f; // (Volitelné pro časové efekty)
}
}

// -------------------- Utilities --------------------
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
        ImGui::SetNextWindowPos(ImVec2((float)Config::Camera::SCREEN_WIDTH - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("Stats", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);
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

// -------------------- OpenGL Wrappers --------------------
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
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
    ~Shader() { if (ID) glDeleteProgram(ID); }
    void use() const { glUseProgram(ID); }
private:
    GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        return shader;
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
std::unique_ptr<Mesh> createSphere(float radius) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int seg = 16;
    for (int y=0;y<=seg;y++){
        float theta = (float)y * glm::pi<float>() / seg;
        for (int x=0;x<=seg;x++){
            float phi = (float)x * 2.0f * glm::pi<float>() / seg;
            vertices.push_back(radius * cos(phi) * sin(theta));
            vertices.push_back(radius * cos(theta));
            vertices.push_back(radius * sin(phi) * sin(theta));
        }
    }
    for (int y=0;y<seg;y++){
        for (int x=0;x<seg;x++){
            int a = y*(seg+1)+x;
            int b = a+seg+1;
            indices.push_back(a); indices.push_back(b); indices.push_back(a+1);
            indices.push_back(b); indices.push_back(b+1); indices.push_back(a+1);
        }
    }
    return std::make_unique<Mesh>(vertices, indices);
}
}

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
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 base = vColor.rgb * 0.5 + vColor.rgb * 0.5 * diff;
    vec3 color = base + vec3(1.0) * 0.5 * spec;
    FragColor = vec4(color, vColor.a);
}
)glsl";


//===========================================================================================================

// -------------------- ECS: Components --------------------
using Entity = std::uint32_t;
const Entity NULL_ENTITY = 0;

enum class TagType { None, Paddle, Ball, Brick, PowerUp };

// --- NOVÁ KOMPONENTA ---
enum class PowerUpType { EnlargePaddle, ExtraLife };

struct PowerUpComponent {
    PowerUpType type;
};
// -----------------------

struct TagComponent { TagType type; };
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 getMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::scale(m, scale);
        return m;
    }
};
struct RigidbodyComponent { glm::vec3 velocity{0.0f}; };
struct ColliderComponent {
    enum Type { Box, Sphere } type;
    float radius = 1.0f;
};
struct RenderComponent {
    Mesh* mesh = nullptr;
    glm::vec4 color{1.0f};
    bool visible = true;
};
struct PlayerControlComponent { float lastX = 0.0f; float velocityX = 0.0f; };
struct GameStateComponent { bool launched = false; };

// -------------------- ECS: Registry --------------------
class Registry {
public:
    std::vector<Entity> entities;
    std::uint32_t nextId = 1;

    std::unordered_map<Entity, TagComponent> tags;
    std::unordered_map<Entity, TransformComponent> transforms;
    std::unordered_map<Entity, RigidbodyComponent> rigidbodies;
    std::unordered_map<Entity, ColliderComponent> colliders;
    std::unordered_map<Entity, RenderComponent> renderables;
    std::unordered_map<Entity, PlayerControlComponent> players;
    std::unordered_map<Entity, GameStateComponent> gameStates;
    std::unordered_map<Entity, PowerUpComponent> powerUps;

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
        powerUps.erase(e);
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
        powerUps.clear();
        nextId = 1;
    }


    template<typename T>
    T& addEntityComponent(Entity e, T component) {

        if constexpr (std::is_same_v<T, TagComponent>) {
            return tags[e] = component;
        } else if constexpr (std::is_same_v<T, TransformComponent>) {
            return transforms[e] = component;
        } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
            return rigidbodies[e] = component;
        } else if constexpr (std::is_same_v<T, ColliderComponent>) {
            return colliders[e] = component;
        } else if constexpr (std::is_same_v<T, RenderComponent>) {
            return renderables[e] = component;
        } else if constexpr (std::is_same_v<T, PlayerControlComponent>) {
            return players[e] = component;
        } else if constexpr (std::is_same_v<T, GameStateComponent>) {
            return gameStates[e] = component;
        } else if constexpr (std::is_same_v<T, PowerUpComponent>) {
            return powerUps[e] = component;
        } else {
            static_assert(std::is_same_v<T, T> && false, "Attempted to add an unhandled component type.");
        }
    }

    template<typename T>
    T* getEntityComponent(Entity e) {

        if constexpr (std::is_same_v<T, TagComponent>) {
            return tags.count(e) ? &tags.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, TransformComponent>) {
            return transforms.count(e) ? &transforms.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, RigidbodyComponent>) {
            return rigidbodies.count(e) ? &rigidbodies.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, ColliderComponent>) {
            return colliders.count(e) ? &colliders.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, RenderComponent>) {
            return renderables.count(e) ? &renderables.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, PlayerControlComponent>) {
            return players.count(e) ? &players.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, GameStateComponent>) {
            return gameStates.count(e) ? &gameStates.at(e) : nullptr;
        } else if constexpr (std::is_same_v<T, PowerUpComponent>) {
            return powerUps.count(e) ? &powerUps.at(e) : nullptr;
        } else {
            return nullptr;
        }
    }
};
// Helpers pro kolize
bool colBoxCircle2D(const glm::vec3 &bPos, const glm::vec3 &bScale, const glm::vec3 &spherePos, float r) {

    float halfW = bScale.x * 0.5f;
    float halfH = bScale.y * 0.5f;
    return (spherePos.x + r > bPos.x - halfW && spherePos.x - r < bPos.x + halfW &&
            spherePos.y + r > bPos.y - halfH && spherePos.y - r < bPos.y + halfH);
}

glm::vec3 reflectVector(const glm::vec3& v, const glm::vec3& normal) {
    return v - 2.0f * glm::dot(v, normal) * normal;
}

bool colBoxBox2D(const glm::vec3& boxPos, const glm::vec3& boxScale, const glm::vec3& otherPos, const glm::vec3& otherScale) {
    float hW1 = boxScale.x * 0.5f;
    float hH1 = boxScale.y * 0.5f;
    float hW2 = otherScale.x * 0.5f;
    float hH2 = otherScale.y * 0.5f;

    return (boxPos.x + hW1 > otherPos.x - hW2 && boxPos.x - hW1 < otherPos.x + hW2 &&
            boxPos.y + hH1 > otherPos.y - hH2 && boxPos.y - hH1 < otherPos.y + hH2);
}

class InputSystem {
public:
    void Update(Registry& registry, GLFWwindow* window, float dt) {
        for (auto& [entity, playerCtrl] : registry.players) {
            auto* transform = registry.getEntityComponent<TransformComponent>(entity);
            if (!transform) continue;

            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            float normX = (float)xpos / width;
            float worldX = Config::World::MIN_X + normX * (Config::World::MAX_X - Config::World::MIN_X);
            float prevX = transform->position.x;
            transform->position.x += (worldX - transform->position.x) * 15.0f * dt;

            // Clamp (s ohledem na aktuální scale - kdyby se pádlo zvětšilo)
            float halfW = transform->scale.x * 0.5f;
            transform->position.x = std::clamp(transform->position.x, Config::World::MIN_X + halfW, Config::World::MAX_X - halfW);

            if (dt > 0.0f) playerCtrl.velocityX = (transform->position.x - prevX) / dt;
            playerCtrl.lastX = transform->position.x;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            for (auto& [entity, state] : registry.gameStates) {
                if (registry.tags[entity].type == TagType::Ball && !state.launched && !registry.globalState.gameOver) {
                    state.launched = true;
                }
            }
        }
    }
};

// --- NOVÝ SYSTÉM PRO POWER-UPY ---
class PowerUpSystem {
public:
    void Update(Registry& reg, float dt) {
        // 1. Najdi pádlo (pro kolizi)
        Entity paddle = NULL_ENTITY;
        TransformComponent* paddleTrans = nullptr;
        for(auto& [e, tag] : reg.tags) {
            if (tag.type == TagType::Paddle) {
                paddle = e;
                paddleTrans = reg.getEntityComponent<TransformComponent>(e);
                break;
            }
        }

        std::vector<Entity> toDestroy;
        // 2. Projdi všechny PowerUpy
        for (auto& [e, pup] : reg.powerUps) {
            auto* trans = reg.getEntityComponent<TransformComponent>(e);
            // Pohyb dolů
            trans->position.y -= Config::PowerUp::FALL_SPEED * dt;
            // Kontrola kolize s pádlem
            if (paddleTrans) {
                // Jednoduché AABB (PowerUp je malý box, pádlo je velký box)
                if (colBoxBox2D(paddleTrans->position, paddleTrans->scale, trans->position, trans->scale)) {
                    ApplyEffect(reg, paddle, pup.type);
                    toDestroy.push_back(e);
                    continue;
                }
            }
            // Smazání pokud propadne
            if (trans->position.y < Config::World::MIN_Y) {
                toDestroy.push_back(e);
            }
        }

        for(auto e : toDestroy) reg.destroyEntity(e);
    }

private:
    void ApplyEffect(Registry& reg, Entity paddle, PowerUpType type) {
        if (type == PowerUpType::EnlargePaddle) {
            auto* trans = reg.getEntityComponent<TransformComponent>(paddle);
            if (trans) {
                trans->scale.x *= 1.3f; // Zvětšení o 30%
                // Omezíme maximální velikost
                if (trans->scale.x > 20.0f) trans->scale.x = 20.0f;
            }
        } else if (type == PowerUpType::ExtraLife) {
            reg.globalState.lives++;
        }
    }
};
// ---------------------------------

class PhysicsSystem {
public:

    Mesh* powerUpMesh = nullptr;

    void Update(Registry& registry, float dt) {
        if (registry.globalState.gameOver) return;

        Entity ballEntity = NULL_ENTITY;
        TransformComponent* ballTrans = nullptr;
        RigidbodyComponent* ballRb = nullptr;
        GameStateComponent* ballState = nullptr;
        ColliderComponent* ballCol = nullptr;
        // Najdeme míček
        for (auto& [e, tag] : registry.tags) {
            if (tag.type == TagType::Ball) {
                ballEntity = e;
                ballTrans = registry.getEntityComponent<TransformComponent>(e);
                ballRb = registry.getEntityComponent<RigidbodyComponent>(e);
                ballState = registry.getEntityComponent<GameStateComponent>(e);
                ballCol = registry.getEntityComponent<ColliderComponent>(e);
                break;
            }
        }

        if (!ballEntity || !ballState) return;
        // Logika před odpálením (Sticky ball)
        if (!ballState->launched) {
            for (auto& [pe, ptag] : registry.tags) {
                if (ptag.type == TagType::Paddle) {
                    auto* pTrans = registry.getEntityComponent<TransformComponent>(pe);
                    if (pTrans) {
                        ballTrans->position.x = pTrans->position.x;
                        ballTrans->position.y = pTrans->position.y + pTrans->scale.y * 0.5f + ballCol->radius + 0.2f;
                    }
                    break;
                }
            }
            return;
        }
        // Pohyb míčku
        ballTrans->position += ballRb->velocity * dt;
        // Odraz od stěn (Walls)
        if (ballTrans->position.x <= Config::World::MIN_X) {
            ballTrans->position.x = Config::World::MIN_X;
            ballRb->velocity.x *= -1.0f;
        }
        else if (ballTrans->position.x >= Config::World::MAX_X) {
            ballTrans->position.x = Config::World::MAX_X;
            ballRb->velocity.x *= -1.0f;
        }
        if (ballTrans->position.y >= Config::World::MAX_Y) {
            ballTrans->position.y = Config::World::MAX_Y;
            ballRb->velocity.y *= -1.0f;
        }
        // Kolize s entitami (Paddle, Bricks)
        std::vector<Entity> destroyedEntities;
        for (auto& [targetE, targetCol] : registry.colliders) {
            if (targetE == ballEntity) continue;
            if (registry.powerUps.count(targetE)) continue; // Ignorujeme powerupy

            auto* targetTrans = registry.getEntityComponent<TransformComponent>(targetE);
            if (!targetTrans) continue;

            // Výpočet AABB polovičních velikostí
            float r = ballCol->radius;
            float halfW = targetTrans->scale.x * 0.5f;
            float halfH = targetTrans->scale.y * 0.5f;

            // Kontrola AABB
            bool hit = (ballTrans->position.x + r > targetTrans->position.x - halfW &&
                        ballTrans->position.x - r < targetTrans->position.x + halfW &&
                        ballTrans->position.y + r > targetTrans->position.y - halfH &&
                        ballTrans->position.y - r < targetTrans->position.y + halfH);

            if (hit) {
                TagComponent* tag = registry.getEntityComponent<TagComponent>(targetE);

                // --- PADDLE ---
                if (tag && tag->type == TagType::Paddle) {
                    // U pádla stačí jednoduchý odraz nahoru
                    ballRb->velocity.y = abs(ballRb->velocity.y); // Vždy nahoru

                    // Faleš
                    auto* playerCtrl = registry.getEntityComponent<PlayerControlComponent>(targetE);
                    if (playerCtrl) ballRb->velocity.x += playerCtrl->velocityX * 0.12f;

                    // Vytlačení nahoru, aby se nezasekl
                    ballTrans->position.y = targetTrans->position.y + halfH + r + 0.05f;
                    applySpeedup(ballRb->velocity);
                }

                //fix tuneling
                // --- BRICK (Zde je ta zásadní oprava) ---
                else if (tag && tag->type == TagType::Brick) {
                    destroyedEntities.push_back(targetE);
                    registry.globalState.score += Config::Stats::SCORE_PER_BRICK;
                    TrySpawnPowerUp(registry, targetTrans->position);
                    // --- OPRAVA KOLIZÍ: Penetration Resolution ---
                    // 1. Vypočítáme, jak hluboko je míček v cihle v obou osách
                    float deltaX = ballTrans->position.x - targetTrans->position.x;
                    float deltaY = ballTrans->position.y - targetTrans->position.y;
                    // Hloubka průniku (Minkowski Sum)
                    float intersectX = abs(deltaX) - (halfW + r);
                    float intersectY = abs(deltaY) - (halfH + r);
                    // Pokud je intersectX větší (blíže k 0, protože jsou záporná čísla při kolizi),
                    // znamená to, že průnik v ose X je MENŠÍ než v ose Y.
                    // Tedy: narazili jsme ze strany.
                    if (intersectX > intersectY) {
                        // Kolize ze strany (Horizontální)
                        // Vytlačíme míček ven v ose X
                        if (deltaX > 0.0f)
                            ballTrans->position.x = targetTrans->position.x + halfW + r;
                        else
                            ballTrans->position.x = targetTrans->position.x - halfW - r;
                        ballRb->velocity.x *= -1.0f; // Odraz X
                    } else {
                        // Kolize z vrchu/spodu (Vertikální)
                        // Vytlačíme míček ven v ose Y
                        if (deltaY > 0.0f)
                            ballTrans->position.y = targetTrans->position.y + halfH + r;
                        else
                            ballTrans->position.y = targetTrans->position.y - halfH - r;

                        ballRb->velocity.y *= -1.0f; // Odraz Y
                    }
                    applySpeedup(ballRb->velocity);

                    break;
                }
            }
        }

        for (auto e : destroyedEntities) registry.destroyEntity(e);
    }
private:
    void TrySpawnPowerUp(Registry& reg, glm::vec3 pos) {
        if (Random::Float(0.0f, 1.0f) < Config::PowerUp::DROP_CHANCE) {
            Entity pup = reg.createEntity();
            PowerUpType type = (Random::Float(0.0f, 1.0f) > 0.5f) ? PowerUpType::EnlargePaddle : PowerUpType::ExtraLife;

            // Barva: Zlutá (Paddle) nebo Zelená (Life)
            glm::vec4 color = (type == PowerUpType::EnlargePaddle)
                                  ? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)
                                  : glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

            reg.addEntityComponent(pup, TagComponent{TagType::PowerUp});
            reg.addEntityComponent(pup, TransformComponent{pos, glm::vec3(2.0f, 0.8f, 1.0f)}); // Plochý obdélník
            reg.addEntityComponent(pup, RenderComponent{powerUpMesh, color});
            reg.addEntityComponent(pup, PowerUpComponent{type});
            // Poznámka: PowerUp nepotřebuje Rigidbody, hýbe s ním PowerUpSystem
            // Ani ColliderComponent pro míček, kolize řeší PowerUpSystem zvlášť
        }
    }


    void applySpeedup(glm::vec3& vel) {
        vel *= Config::Ball::SPEEDUP_FACTOR;
        if (glm::length(vel) > Config::Ball::MAX_SPEED) vel = glm::normalize(vel) * Config::Ball::MAX_SPEED;
    }
};

class GameLogicSystem {
public:
    void Update(Registry& registry) {
        Entity ballEntity = NULL_ENTITY;
        for (auto& [e, tag] : registry.tags) if (tag.type == TagType::Ball) ballEntity = e;

        if (ballEntity != NULL_ENTITY) {
            auto* trans = registry.getEntityComponent<TransformComponent>(ballEntity);
            if (trans && trans->position.y < Config::World::MIN_Y) {
                registry.globalState.lives--;
                if (registry.globalState.lives <= 0) {
                    registry.globalState.gameOver = true;
                } else {
                    resetRound(registry, ballEntity);
                }
            }
        }
        bool anyBrick = false;
        for (auto& [e, tag] : registry.tags) { if (tag.type == TagType::Brick) { anyBrick = true; break; } }
        if (!anyBrick) { registry.globalState.gameWon = true; registry.globalState.gameOver = true; }
    }

    void resetRound(Registry& registry, Entity ball) {
        auto* ballState = registry.getEntityComponent<GameStateComponent>(ball);
        auto* ballRb = registry.getEntityComponent<RigidbodyComponent>(ball);

        if (ballState) ballState->launched = false;
        if (ballRb) ballRb->velocity = Config::Ball::START_VEL;

        // Reset Paddle Position and Scale
        for(auto& [e, tag] : registry.tags) {
            if(tag.type == TagType::Paddle) {
                auto* pt = registry.getEntityComponent<TransformComponent>(e);
                if(pt) {
                    pt->position = Config::Paddle::START_POS;
                    pt->scale = Config::Paddle::SCALE; // Reset velikosti při smrti
                }
                auto* pc = registry.getEntityComponent<PlayerControlComponent>(e);
                if(pc) { pc->velocityX = 0; pc->lastX = Config::Paddle::START_POS.x; }
            }
        }

        // Smazat padající powerupy při resetu
        std::vector<Entity> toRemove;
        for(auto& [e, p] : registry.powerUps) toRemove.push_back(e);
        for(auto e : toRemove) registry.destroyEntity(e);
    }
};

class RenderSystem {

    std::unique_ptr<Buffer> vboInstance;
    std::unique_ptr<Buffer> vboColor;
    std::vector<glm::mat4> matrices;
    std::vector<glm::vec4> colors;

public:
    void Init() {
        size_t maxInstances = 2000;
        vboInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboInstance->allocate(maxInstances * sizeof(glm::mat4));
        vboColor = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboColor->allocate(maxInstances * sizeof(glm::vec4));
    }
    void SetupVAO(const VertexArray& vao) {
        vao.bind();
        vboInstance->bind();
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(1 + i, 1);
        }
        vboColor->bind();
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
        glVertexAttribDivisor(5, 1);
        vao.unbind();
    }

    void Update(Registry& registry, Shader& shader) {

        shader.use();
        std::unordered_map<Mesh*, std::vector<Entity>> batch;

        for (auto& [e, rend] : registry.renderables) {
            if (!rend.visible || !rend.mesh) continue;
            batch[rend.mesh].push_back(e);
        }
        for (auto& [mesh, entities] : batch) {
            matrices.clear(); colors.clear();
            for (Entity e : entities) {
                auto* trans = registry.getEntityComponent<TransformComponent>(e);
                auto* rend = registry.getEntityComponent<RenderComponent>(e);
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

    void DrawUI(Registry& registry, Stats& stats, std::function<void()> onRestart, std::function<void()> onQuit) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        stats.drawUI();

        // Malé info okno
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("GameInfo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Score: %d", registry.globalState.score);
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Lives: %d", registry.globalState.lives);
        ImGui::End();

        // Logika pro otevření pop-upu
        if (registry.globalState.gameOver) {
            // Otevřeme popup jen pokud ještě není otevřený
            if (!ImGui::IsPopupOpen("GameOver") && !ImGui::IsPopupOpen("GameWon")) {
                ImGui::OpenPopup(registry.globalState.gameWon ? "GameWon" : "GameOver");
            }
        }

        // --- GAME OVER POPUP ---
        if (ImGui::BeginPopupModal("GameOver", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("GAME OVER");
            ImGui::Text("Final Score: %d", registry.globalState.score);
            ImGui::Separator();

            // Tlačítko RESTART
            if (ImGui::Button("Restart Game", ImVec2(120, 0))) {
                onRestart(); // Voláme callback
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();

            // Tlačítko QUIT
            if (ImGui::Button("Quit App", ImVec2(120, 0))) {
                onQuit(); // Voláme callback
            }
            ImGui::EndPopup();
        }

        // --- GAME WON POPUP ---
        if (ImGui::BeginPopupModal("GameWon", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "CONGRATULATIONS!");
            ImGui::Text("You destroyed all bricks!");
            ImGui::Text("Final Score: %d", registry.globalState.score);
            ImGui::Separator();

            if (ImGui::Button("Play Again", ImVec2(120, 0))) {
                onRestart();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();

            if (ImGui::Button("Quit App", ImVec2(120, 0))) {
                onQuit();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
};
// -------------------- Main Class --------------------
class Game {
    struct WindowDeleter { void operator()(GLFWwindow* ptr) { glfwDestroyWindow(ptr); } };
    std::unique_ptr<GLFWwindow, WindowDeleter> window;

    std::unique_ptr<Shader> shader;
    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<Mesh> sphereMesh;
    std::unique_ptr<Buffer> uboCamera;

    Registry registry;
    InputSystem inputSystem;
    PhysicsSystem physicsSystem;
    RenderSystem renderSystem;
    GameLogicSystem logicSystem;
    PowerUpSystem powerUpSystem; // Instance nového systému

    Stats stats;
    glm::mat4 view, proj;

public:
    bool init() {
        if (!glfwInit()) return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window.reset(glfwCreateWindow(Config::Camera::SCREEN_WIDTH, Config::Camera::SCREEN_HEIGHT, "Arkanoid ECS + PowerUps", nullptr, nullptr));
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
        view = glm::lookAt(
            Config::Camera::CAMERA_POS,
            Config::Camera::CAMERA_POS + Config::Camera::CAMERA_FRONT,
            Config::Camera::CAMERA_UP
            );

        proj = glm::perspective(
            glm::radians(45.0f),
            (float)Config::Camera::SCREEN_WIDTH / Config::Camera::SCREEN_HEIGHT,
            0.1f, 100.0f
            );
        return true;
    }

    void run() {

        //  FIXED dt use
        const float FIXED_DT = 1.0f / 120.0f; // 120 Hz
        float accumulator = 0.0f;
        float lastTime = (float)glfwGetTime();

        while (!glfwWindowShouldClose(window.get())) {
            float now = (float)glfwGetTime();
            float frameTime = std::min(now - lastTime, 0.05f); // ochrana proti SPIRAL OF DEATH
            lastTime = now;
            accumulator += frameTime;
            glfwPollEvents();
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window.get(), true);

            if (registry.globalState.gameOver &&
                glfwGetKey(window.get(), GLFW_KEY_R) == GLFW_PRESS)
                resetGame();

            inputSystem.Update(registry, window.get(), frameTime);
            //  FIXED dt use
            while (accumulator >= FIXED_DT) {
                if (!registry.globalState.gameOver) {
                    physicsSystem.Update(registry, FIXED_DT);
                    powerUpSystem.Update(registry, FIXED_DT);
                    logicSystem.Update(registry);
                }
                accumulator -= FIXED_DT;
            }
            uboCamera->bind();
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));
            uboCamera->unbind();
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderSystem.Update(registry, *shader);
            stats.update(frameTime);
            renderSystem.DrawUI(
                registry,
                stats,
                [&]() { this->resetGame(); },
                [&]() { glfwSetWindowShouldClose(window.get(), true); }
                );

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
        physicsSystem.powerUpMesh = cubeMesh.get();
    }

    void resetGame() {
        registry.clear();
        registry.globalState = {0, Config::Stats::INITIAL_LIVES, false, false};

        // Paddle
        Entity paddle = registry.createEntity();
        registry.addEntityComponent(paddle, TagComponent{TagType::Paddle});
        registry.addEntityComponent(paddle, TransformComponent{Config::Paddle::START_POS, Config::Paddle::SCALE});
        registry.addEntityComponent(paddle, RenderComponent{cubeMesh.get(), glm::vec4(0.3f, 0.8f, 0.3f, 1.0f)});
        registry.addEntityComponent(paddle, ColliderComponent{ColliderComponent::Box});
        registry.addEntityComponent(paddle, PlayerControlComponent{});

        // Ball
        Entity ball = registry.createEntity();
        registry.addEntityComponent(ball, TagComponent{TagType::Ball});
        registry.addEntityComponent(ball, TransformComponent{Config::Ball::START_POS, glm::vec3(Config::Ball::RADIUS)});
        registry.addEntityComponent(ball, RigidbodyComponent{Config::Ball::START_VEL});
        registry.addEntityComponent(ball, RenderComponent{sphereMesh.get(), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f)});
        registry.addEntityComponent(ball, ColliderComponent{ColliderComponent::Sphere, Config::Ball::RADIUS});
        registry.addEntityComponent(ball, GameStateComponent{false});

        // Bricks
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
                registry.addEntityComponent(brick, TagComponent{TagType::Brick});
                registry.addEntityComponent(brick, TransformComponent{pos, scale});
                registry.addEntityComponent(brick, RenderComponent{cubeMesh.get(), Random::RandomColor()});
                registry.addEntityComponent(brick, ColliderComponent{ColliderComponent::Box});
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
