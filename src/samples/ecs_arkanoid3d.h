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
#include <unordered_map>
#include <typeindex>
#include <optional>
#include <functional>

// ==================== CONFIGURATION ====================
namespace Config {
constexpr unsigned int SCREEN_WIDTH = 1280;
constexpr unsigned int SCREEN_HEIGHT = 720;

namespace World {
constexpr float MIN_X = -60.0f;
constexpr float MAX_X = 60.0f;
constexpr float MIN_Y = -40.0f;
constexpr float MAX_Y = 20.0f;
}

namespace Bricks {
constexpr int ROWS = 10;
constexpr int COLS = 100;
constexpr float START_Y = 2.0f;
constexpr glm::vec3 SCALE = { 2.5f, 1.0f, 1.0f }; // Width se dopočítá dynamicky, ale scale Y/Z fixní
}

namespace Paddle {
constexpr glm::vec3 START_POS = { 0.0f, -30.0f, 0.0f };
constexpr glm::vec3 SCALE = { 10.0f, 2.0f, 2.0f };
}

namespace Ball {
constexpr glm::vec3 START_POS = { 0.0f, -25.0f, 0.0f };
constexpr glm::vec3 START_VEL = { 10.0f, 16.0f, 0.0f };
constexpr float RADIUS = 1.0f;
constexpr float SPEEDUP_FACTOR = 1.05f;
constexpr float MAX_SPEED = 40.0f;
}

constexpr int INITIAL_LIVES = 3;
constexpr int SCORE_PER_BRICK = 10;
}

// ==================== UTILITIES & WRAPPERS ====================
// (Zachováno z původního kódu - low level OpenGL)

class Random {
public:
    static float Float(float min, float max) {
        static std::mt19937 mt{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(min, max);
        return dist(mt);
    }
    static glm::vec4 RandomColor() {
        return glm::vec4(Float(0.2f, 1.0f), Float(0.2f, 1.0f), Float(0.2f, 1.0f), 1.0f);
    }
};

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
        glDeleteShader(vs); glDeleteShader(fs);
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
    GLuint ID = 0; GLenum type;
    Buffer(GLenum type) : type(type) { glGenBuffers(1, &ID); }
    ~Buffer() { if (ID) glDeleteBuffers(1, &ID); }
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
    ~VertexArray() { if (ID) glDeleteVertexArrays(1, &ID); }
    void bind() const { glBindVertexArray(ID); }
    void unbind() const { glBindVertexArray(0); }
};

struct Mesh {
    std::unique_ptr<VertexArray> vao;
    std::unique_ptr<Buffer> vbo, ebo;
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
};

// ==================== ECS CORE ====================

// 1. Component Interface (Polymorfní base, aby šly ukládat do mapy)
struct Component {
    virtual ~Component() = default;
};

// 2. Entity
class Entity {
public:
    using ID = size_t;
private:
    ID id;
    static inline ID idCounter = 0;
    // Mapa typ -> pointer na komponentu. Jednoduché a efektivní pro tuto velikost.
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;
    bool active = true;

public:
    Entity() : id(idCounter++) {}

    ID getID() const { return id; }
    bool isActive() const { return active; }
    void destroy() { active = false; }

    template <typename T, typename... Args>
    T& addComponent(Args&&... args) {
        components[typeid(T)] = std::make_unique<T>(std::forward<Args>(args)...);
        return *static_cast<T*>(components[typeid(T)].get());
    }

    template <typename T>
    T& getComponent() {
        return *static_cast<T*>(components.at(typeid(T)).get());
    }

    template <typename T>
    bool hasComponent() const {
        return components.find(typeid(T)) != components.end();
    }
};

// ==================== COMPONENTS ====================

struct CTransform : public Component {
    glm::vec3 pos{0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 getMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, pos);
        m = glm::scale(m, scale);
        return m;
    }
};

struct CRigidBody : public Component {
    glm::vec3 velocity{0.0f};
    bool isStatic = false;
};

struct CCollider : public Component {
    enum Type { BOX, SPHERE } type;
    glm::vec3 boxSize{1.0f}; // Pro BOX (odpovídá scale)
    float radius{1.0f};      // Pro SPHERE
    bool isTrigger = false;  // Pokud true, neřeší fyzickou odezvu, jen event
};

struct CRenderable : public Component {
    Mesh* meshPtr = nullptr; // Odkaz na mesh v ResourceManageru (ne vlastnictví)
    glm::vec4 color{1.0f};
};

struct CTag : public Component {
    std::string tag;
    CTag(std::string t) : tag(std::move(t)) {}
};

// ==================== RESOURCE MANAGER ====================
// Drží shadery a meshe, aby je entity nevlastnily
class ResourceManager {
public:
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<Mesh> sphereMesh;

    void init() {
        // Shaders (Stejné jako v původním kódu)
        const char* VS = R"glsl(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec4 aRow0; layout(location = 2) in vec4 aRow1;
            layout(location = 3) in vec4 aRow2; layout(location = 4) in vec4 aRow3;
            layout(location = 5) in vec4 aColor;
            layout(std140) uniform Camera { mat4 view; mat4 projection; };
            out vec3 vPos; out vec3 vNormal; out vec4 vColor;
            void main() {
                mat4 model = mat4(aRow0, aRow1, aRow2, aRow3);
                vec4 worldPos = model * vec4(aPos, 1.0);
                vPos = worldPos.xyz;
                vNormal = normalize(mat3(model) * aPos);
                vColor = aColor;
                gl_Position = projection * view * worldPos;
            }
        )glsl";
        const char* FS = R"glsl(
            #version 330 core
            in vec3 vPos; in vec3 vNormal; in vec4 vColor;
            out vec4 FragColor;
            void main() {
                vec3 N = normalize(vNormal);
                vec3 L = normalize(vec3(10.0, 20.0, 10.0) - vPos);
                float diff = max(dot(N, L), 0.2); // Ambient 0.2
                FragColor = vec4(vColor.rgb * diff, vColor.a);
            }
        )glsl";
        shader = std::make_unique<Shader>(VS, FS);

        // Meshes
        cubeMesh = createCube();
        sphereMesh = createSphere();
    }

private:
    std::unique_ptr<Mesh> createCube() {
        std::vector<float> v = {
            -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f,
            -0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f
        };
        std::vector<unsigned int> i = {
            0,1,2, 2,3,0, 4,5,6, 6,7,4, 0,1,5, 5,4,0, 2,3,7, 7,6,2, 0,3,7, 7,4,0, 1,2,6, 6,5,1
        };
        return std::make_unique<Mesh>(v, i);
    }
    std::unique_ptr<Mesh> createSphere() {
        // Zjednodušená sféra
        std::vector<float> v; std::vector<unsigned int> i;
        int lat = 12, lon = 12; float r = 1.0f;
        for(int y=0; y<=lat; y++) {
            float theta = y * 3.14159f / lat;
            for(int x=0; x<=lon; x++) {
                float phi = x * 2 * 3.14159f / lon;
                v.push_back(r*sin(theta)*cos(phi)); v.push_back(r*cos(theta)); v.push_back(r*sin(theta)*sin(phi));
            }
        }
        for(int y=0; y<lat; y++) {
            for(int x=0; x<lon; x++) {
                int a = y*(lon+1)+x, b = a+lon+1;
                i.insert(i.end(), { (unsigned int)a, (unsigned int)b, (unsigned int)a+1, (unsigned int)b, (unsigned int)b+1, (unsigned int)a+1 });
            }
        }
        return std::make_unique<Mesh>(v, i);
    }
};

// ==================== SCENE ====================
class Scene {
public:
    std::vector<std::shared_ptr<Entity>> entities;
    std::vector<std::shared_ptr<Entity>> entitiesToAdd; // Double buffer pro bezpečné přidávání

    std::shared_ptr<Entity> createEntity() {
        auto e = std::make_shared<Entity>();
        entitiesToAdd.push_back(e);
        return e;
    }

    void updateLists() {
        // Add new
        if(!entitiesToAdd.empty()){
            entities.insert(entities.end(), entitiesToAdd.begin(), entitiesToAdd.end());
            entitiesToAdd.clear();
        }
        // Remove dead (Erase-Remove idiom)
        entities.erase(std::remove_if(entities.begin(), entities.end(),
                                      [](const auto& e) { return !e->isActive(); }), entities.end());
    }

    void clear() {
        entities.clear();
        entitiesToAdd.clear();
    }

    // Helper: Vrátí view na entity, které mají všechny zadané komponenty
    template<typename... Comps>
    auto view() {
        std::vector<Entity*> result;
        for(auto& e : entities) {
            if ((e->hasComponent<Comps>() && ...)) {
                result.push_back(e.get());
            }
        }
        return result;
    }
};

// ==================== SYSTEMS ====================

// --- INPUT SYSTEM ---
class InputSystem {
public:
    void update(Scene& scene, GLFWwindow* window, float dt) {
        auto paddles = scene.view<CTag, CTransform>();
        for (auto* e : paddles) {
            if (e->getComponent<CTag>().tag == "Paddle") {
                auto& trans = e->getComponent<CTransform>();

                double xpos;
                glfwGetCursorPos(window, &xpos, nullptr);
                int width;
                glfwGetWindowSize(window, &width, nullptr);

                float normX = (float)xpos / width;
                float worldX = Config::World::MIN_X + normX * (Config::World::MAX_X - Config::World::MIN_X);

                // Smooth follow
                trans.pos.x += (worldX - trans.pos.x) * 15.0f * dt;

                // Clamp
                float halfW = trans.scale.x * 0.5f;
                trans.pos.x = std::clamp(trans.pos.x, Config::World::MIN_X + halfW, Config::World::MAX_X - halfW);
            }
        }
    }
};

// --- PHYSICS SYSTEM ---
class PhysicsSystem {
public:
    // Callbacks pro komunikaci s herní logikou
    std::function<void(int)> onScore;
    std::function<void()> onBallLost;

    void update(Scene& scene, float dt) {
        // 1. Pohyb (Integration)
        for (auto* e : scene.view<CRigidBody, CTransform>()) {
            auto& rb = e->getComponent<CRigidBody>();
            if (rb.isStatic) continue;

            auto& tr = e->getComponent<CTransform>();
            tr.pos += rb.velocity * dt;
        }

        // 2. Kolize Míče (Specifická logika pro Arkanoid)
        Entity* ball = nullptr;
        Entity* paddle = nullptr;

        // Najdeme klíčové entity
        for(auto* e : scene.view<CTag>()) {
            if(e->getComponent<CTag>().tag == "Ball") ball = e;
            if(e->getComponent<CTag>().tag == "Paddle") paddle = e;
        }

        if(!ball) return;

        handleWallCollisions(ball);
        if (paddle) handlePaddleCollision(ball, paddle);
        handleBrickCollisions(scene, ball);
    }

private:
    bool checkAABB(const glm::vec3& boxPos, const glm::vec3& boxScale, const glm::vec3& spherePos, float radius) {
        float halfW = boxScale.x * 0.5f;
        float halfH = boxScale.y * 0.5f;
        return (spherePos.x + radius > boxPos.x - halfW && spherePos.x - radius < boxPos.x + halfW &&
                spherePos.y + radius > boxPos.y - halfH && spherePos.y - radius < boxPos.y + halfH);
    }

    void handleWallCollisions(Entity* ball) {
        auto& tr = ball->getComponent<CTransform>();
        auto& rb = ball->getComponent<CRigidBody>();

        if (tr.pos.x <= Config::World::MIN_X || tr.pos.x >= Config::World::MAX_X) rb.velocity.x *= -1.0f;
        if (tr.pos.y >= Config::World::MAX_Y) rb.velocity.y *= -1.0f;

        if (tr.pos.y < Config::World::MIN_Y) {
            if(onBallLost) onBallLost();
        }
    }

    void handlePaddleCollision(Entity* ball, Entity* paddle) {
        auto& bTr = ball->getComponent<CTransform>();
        auto& bRb = ball->getComponent<CRigidBody>();
        auto& bCol = ball->getComponent<CCollider>();

        auto& pTr = paddle->getComponent<CTransform>();
        auto& pCol = paddle->getComponent<CCollider>(); // Box

        if (checkAABB(pTr.pos, pCol.boxSize, bTr.pos, bCol.radius)) {
            float diff = bTr.pos.x - pTr.pos.x;
            float percent = diff / (pCol.boxSize.x * 0.5f);

            bRb.velocity.x = percent * 10.0f;
            bRb.velocity.y = std::abs(bRb.velocity.y); // Vždy nahoru
            bTr.pos.y = pTr.pos.y + pCol.boxSize.y * 0.5f + bCol.radius + 0.1f;

            // Speed up
            bRb.velocity *= Config::Ball::SPEEDUP_FACTOR;
            if(glm::length(bRb.velocity) > Config::Ball::MAX_SPEED)
                bRb.velocity = glm::normalize(bRb.velocity) * Config::Ball::MAX_SPEED;
        }
    }

    void handleBrickCollisions(Scene& scene, Entity* ball) {
        auto& bTr = ball->getComponent<CTransform>();
        auto& bRb = ball->getComponent<CRigidBody>();
        auto& bCol = ball->getComponent<CCollider>();

        // Optimalizace: V reálném enginu použij QuadTree. Tady iterujeme.
        for (auto* e : scene.view<CTag, CTransform, CCollider>()) {
            if (e->getComponent<CTag>().tag == "Brick" && e->isActive()) {
                auto& brTr = e->getComponent<CTransform>();
                auto& brCol = e->getComponent<CCollider>();

                if (checkAABB(brTr.pos, brCol.boxSize, bTr.pos, bCol.radius)) {
                    e->destroy(); // Označit ke smazání
                    if(onScore) onScore(Config::SCORE_PER_BRICK);

                    glm::vec3 delta = bTr.pos - brTr.pos;
                    if (abs(delta.x) > abs(delta.y)) bRb.velocity.x *= -1.0f;
                    else bRb.velocity.y *= -1.0f;

                    break; // Max 1 kolize za frame proti tunelování
                }
            }
        }
    }
};

// --- RENDER SYSTEM ---
class RenderSystem {
    std::unique_ptr<Buffer> uboCamera;
    std::unique_ptr<Buffer> vboInstance;
    std::unique_ptr<Buffer> vboColor;

    // Batching struktura
    struct Batch {
        Mesh* mesh;
        std::vector<glm::mat4> matrices;
        std::vector<glm::vec4> colors;
    };

public:
    void init() {
        uboCamera = std::make_unique<Buffer>(GL_UNIFORM_BUFFER);
        uboCamera->allocate(2 * sizeof(glm::mat4));
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboCamera->ID);

        // Pre-allocate instance buffers (velký buffer, aby se nemuselo realokovat)
        vboInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboInstance->allocate(2000 * sizeof(glm::mat4));
        vboColor = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboColor->allocate(2000 * sizeof(glm::vec4));
    }

    void setupInstancedAttributes(VertexArray& vao) {
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

    void render(Scene& scene, ResourceManager& res, float aspectRatio) {
        // 1. Update Camera
        static const glm::vec3 camPos = {0.0f, 4.0f, 95.0f};
        glm::mat4 view = glm::lookAt(camPos, {0.0f, -0.1f, -1.0f}, {0.0f, 1.0f, 0.0f});
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 150.0f);

        uboCamera->bind();
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));
        uboCamera->unbind();

        // 2. Prepare Batches (ECS Magic: Seskupit podle meshe)
        std::unordered_map<Mesh*, Batch> batches;

        for(auto* e : scene.view<CRenderable, CTransform>()) {
            auto& ren = e->getComponent<CRenderable>();
            auto& tr = e->getComponent<CTransform>();

            if(!ren.meshPtr) continue;

            batches[ren.meshPtr].mesh = ren.meshPtr;
            batches[ren.meshPtr].matrices.push_back(tr.getMatrix());
            batches[ren.meshPtr].colors.push_back(ren.color);
        }

        // 3. Draw Batches
        res.shader->use();

        // Ensure VAO has instance attributes (Lazy init - jen jednou)
        static bool setupDone = false;
        if(!setupDone) {
            setupInstancedAttributes(*res.cubeMesh->vao);
            setupInstancedAttributes(*res.sphereMesh->vao);
            setupDone = true;
        }

        for (auto& [mesh, batch] : batches) {
            if(batch.matrices.empty()) continue;

            // Upload dat pro instancing
            vboInstance->setSubData(batch.matrices);
            vboColor->setSubData(batch.colors);

            // Draw
            mesh->vao->bind();
            glDrawElementsInstanced(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0, (GLsizei)batch.matrices.size());
            mesh->vao->unbind();
        }
    }
};

// --- GAME LOGIC SYSTEM ---
// Řídí pravidla hry, respawn, skóre
class GameLogicSystem {
public:
    int lives = Config::INITIAL_LIVES;
    int score = 0;
    bool gameOver = false;

    void initGame(Scene& scene, ResourceManager& res) {
        scene.clear();
        score = 0;
        lives = Config::INITIAL_LIVES;
        gameOver = false;

        createLevel(scene, res);
        spawnPaddle(scene, res);
        spawnBall(scene, res);
        scene.updateLists(); // Aplikuj změny
    }

    void resetBall(Scene& scene, ResourceManager& res) {
        // Smaž starý míč
        for(auto* e : scene.view<CTag>()) {
            if(e->getComponent<CTag>().tag == "Ball") e->destroy();
        }
        scene.updateLists();
        spawnBall(scene, res);
    }

private:
    void createLevel(Scene& scene, ResourceManager& res) {
        int rows = Config::Bricks::ROWS;
        int cols = Config::Bricks::COLS;
        float totalW = Config::World::MAX_X - Config::World::MIN_X;
        float bW = totalW / cols;
        float bH = Config::Bricks::SCALE.y;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                auto br = scene.createEntity();
                br->addComponent<CTag>("Brick");

                auto& tr = br->addComponent<CTransform>();
                tr.pos = { Config::World::MIN_X + bW/2 + c*bW, Config::Bricks::START_Y + r*(bH+0.5f), 0.0f };
                tr.scale = { bW - 0.5f, bH, 2.0f };

                auto& ren = br->addComponent<CRenderable>();
                ren.meshPtr = res.cubeMesh.get();
                ren.color = Random::RandomColor();

                auto& col = br->addComponent<CCollider>();
                col.type = CCollider::BOX;
                col.boxSize = tr.scale;
            }
        }
    }

    void spawnPaddle(Scene& scene, ResourceManager& res) {
        auto p = scene.createEntity();
        p->addComponent<CTag>("Paddle");

        auto& tr = p->addComponent<CTransform>();
        tr.pos = Config::Paddle::START_POS;
        tr.scale = Config::Paddle::SCALE;

        auto& ren = p->addComponent<CRenderable>();
        ren.meshPtr = res.cubeMesh.get();
        ren.color = {0.2f, 0.8f, 0.2f, 1.0f};

        auto& rb = p->addComponent<CRigidBody>();
        rb.isStatic = true; // Ovládáno Input systémem, ne fyzikou

        auto& col = p->addComponent<CCollider>();
        col.type = CCollider::BOX;
        col.boxSize = tr.scale;
    }

    void spawnBall(Scene& scene, ResourceManager& res) {
        auto b = scene.createEntity();
        b->addComponent<CTag>("Ball");

        auto& tr = b->addComponent<CTransform>();
        tr.pos = Config::Ball::START_POS;
        tr.scale = glm::vec3(Config::Ball::RADIUS);

        auto& ren = b->addComponent<CRenderable>();
        ren.meshPtr = res.sphereMesh.get();
        ren.color = {1.0f, 0.2f, 0.2f, 1.0f};

        auto& rb = b->addComponent<CRigidBody>();
        rb.velocity = Config::Ball::START_VEL;
        rb.isStatic = false;

        auto& col = b->addComponent<CCollider>();
        col.type = CCollider::SPHERE;
        col.radius = Config::Ball::RADIUS;
    }
};

// ==================== MAIN APP ====================
class Game {
    GLFWwindow* window;
    ResourceManager resources;
    Scene scene;

    // Instance Systémů
    InputSystem inputSystem;
    PhysicsSystem physicsSystem;
    RenderSystem renderSystem;
    GameLogicSystem gameLogic;

public:
    bool init() {
        if (!glfwInit()) return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, "OOP ECS Arkanoid", nullptr, nullptr);
        if (!window) return false;
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // VSync ON
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;
        glEnable(GL_DEPTH_TEST);

        // ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        // Init Engine
        resources.init();
        renderSystem.init();

        // Bind Events (Fyzika hlásí události Logic systému)
        physicsSystem.onScore = [&](int points) { gameLogic.score += points; };
        physicsSystem.onBallLost = [&]() {
            gameLogic.lives--;
            if (gameLogic.lives <= 0) gameLogic.gameOver = true;
            else gameLogic.resetBall(scene, resources);
        };

        // Start Game
        gameLogic.initGame(scene, resources);
        return true;
    }

    void run() {
        float lastTime = (float)glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
            float now = (float)glfwGetTime();
            float dt = now - lastTime;
            lastTime = now;

            glfwPollEvents();

            if (!gameLogic.gameOver) {
                inputSystem.update(scene, window, dt);
                physicsSystem.update(scene, dt);
                scene.updateLists(); // Clean up dead entities
            }

            render();
        }
        cleanup();
    }

private:
    void render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // RenderSystem vše zařídí (včetně instancingu)
        renderSystem.render(scene, resources, (float)Config::SCREEN_WIDTH/Config::SCREEN_HEIGHT);
        renderUI();

        glfwSwapBuffers(window);
    }

    void renderUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        ImGui::Text("Score: %d", gameLogic.score);
        ImGui::Text("Lives: %d", gameLogic.lives);
        ImGui::Text("Entities: %zu", scene.entities.size());
        ImGui::End();

        if (gameLogic.gameOver) {
            ImGui::OpenPopup("GameOver");
        }
        if (ImGui::BeginPopupModal("GameOver", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("GAME OVER");
            if (ImGui::Button("Restart")) {
                gameLogic.initGame(scene, resources);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit")) glfwSetWindowShouldClose(window, true);
            ImGui::EndPopup();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    Game game;
    if (game.init()) {
        game.run();
    } else {
        //std::cerr << "Failed to initialize game." << std::endl;
        return -1;
    }
    return 0;
}
