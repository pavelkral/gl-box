// modern_ecs_opengl.cpp
// Kompletní single-file projekt: ECS + OpenGL + ImGui
// Kompilovat s GLAD, GLFW, GLM, ImGui dle instrukcí výše.


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <random>
#include <string>

struct Stats {
    float deltaTime = 0.0f;      // poslední frame time
    int frameCount = 0;          // počítadlo frame
    float fpsTimer = 0.0f;       // akumulátor času
    float fps = 0.0f;            // FPS za poslední sekundu

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
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
        ImGui::End();
    }
};
// -------------------- Config --------------------
static constexpr glm::vec3 CAMERA_POS   = {0.0f, 4.0f, 95.0f};
static constexpr glm::vec3 CAMERA_FRONT = {0.0f, -0.1f, -1.0f};
static constexpr glm::vec3 CAMERA_UP    = {0.0f, 1.0f, 0.0f};

constexpr unsigned int SCR_WIDTH = 1280;
constexpr unsigned int SCR_HEIGHT = 720;
constexpr unsigned int MAX_ENTITIES = 16384; // dostatečně pro 4000 bricks + paddle + balls
using Entity = uint32_t;
const Entity INVALID_ENTITY = UINT32_MAX;
constexpr GLuint CAMERA_UBO_BINDING = 0;

namespace Config {
namespace World {
constexpr float MIN_X = -60.0f;
constexpr float MAX_X = 60.0f;
constexpr float MIN_Y = -40.0f;
constexpr float MAX_Y = 40.0f;
}
namespace Bricks {
constexpr int ROWS = 10;
constexpr int COLS = 30;
// šířka hracího pole
constexpr float FIELD_WIDTH = Config::World::MAX_X - Config::World::MIN_X;
// šířka jednoho bricku tak, aby vyplnil šířku
constexpr float SCALE_X = FIELD_WIDTH / COLS;
// fixní výška bricku (např. původní 1.0f)
constexpr float SCALE_Y = 1.0f;
// start levý horní roh
constexpr float START_X = Config::World::MIN_X + SCALE_X*0.5f;
constexpr float START_Y = 2.0f; // původní START_Y
constexpr float SPACING_X = 0.0f;
constexpr float SPACING_Y = 0.5f; // volitelné odsazení mezi řadami
constexpr glm::vec3 SCALE = {SCALE_X - SPACING_X, SCALE_Y, 1.0f};
}
namespace Paddle {
constexpr glm::vec3 START_POS = {0.0f, -30.0f, 0.0f};
constexpr glm::vec3 SCALE = {10.0f, 2.0f, 2.0f};
constexpr float SPEED = 50.0f;
}
namespace Ball {
constexpr glm::vec3 START_POS = {0.0f, -25.0f, 0.0f};
constexpr glm::vec3 START_VEL = {10.0f, 16.0f, 0.0f};
constexpr float RADIUS = 1.0f;
constexpr float SPEEDUP_FACTOR = 1.05f; // každá kolize zrychlí o 5%
constexpr float MAX_SPEED = 30.0f;      // maximální rychlost
}
constexpr int INITIAL_LIVES = 3;
constexpr int SCORE_PER_BRICK = 10;
}

// -------------------- Random --------------------
static std::mt19937 rng{std::random_device{}()};
static float frand(float a = 0.0f, float b = 1.0f){
    std::uniform_real_distribution<float> d(a,b);
    return d(rng);
}
static glm::vec4 randColor(){
    return glm::vec4(frand(0.3f,1.0f), frand(0.3f,1.0f), frand(0.3f,1.0f), 1.0f);
}

// -------------------- Minimal shader helpers --------------------
static void checkShaderCompile(GLuint sh, const char* name){
    GLint ok=GL_FALSE; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok){ char buf[1024]; glGetShaderInfoLog(sh, 1024, NULL, buf); std::cerr<<"Shader "<<name<<" compile error:\n"<<buf<<"\n"; }
}
static void checkProgramLink(GLuint p, const char* name){
    GLint ok=GL_FALSE; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ char buf[1024]; glGetProgramInfoLog(p, 1024, NULL, buf); std::cerr<<"Program "<<name<<" link error:\n"<<buf<<"\n"; }
}

const char* vertexSrc = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aModelRow0;
layout(location=2) in vec4 aModelRow1;
layout(location=3) in vec4 aModelRow2;
layout(location=4) in vec4 aModelRow3;
layout(location=5) in vec4 aColor;

layout(std140) uniform Camera {
    mat4 view;
    mat4 projection;
};

out vec4 vColor;
out vec3 vWorldPos;
out vec3 Normal;

void main(){
    mat4 model = mat4(aModelRow0, aModelRow1, aModelRow2, aModelRow3);
    vec4 worldPos = model * vec4(aPos,1.0);
    vWorldPos = worldPos.xyz;
    vColor = aColor;
    Normal = normalize(mat3(model) * aPos);
    gl_Position = projection * view * worldPos;
}
)glsl";

const char* fragmentSrc = R"glsl(
#version 330 core
in vec3 vWorldPos;
in vec3 Normal;
in vec4 vColor;
out vec4 FragColor;
void main(){
    vec3 N = normalize(Normal);
    vec3 lightPos = vec3(50.0,100.0,50.0);
    vec3 L = normalize(lightPos - vWorldPos);
    vec3 V = normalize(vec3(0.0,50.0,100.0) - vWorldPos);
    float diff = max(dot(N,L), 0.0);
    vec3 H = normalize(L+V);
    float spec = pow(max(dot(N,H),0.0),64.0);
    vec3 base = vColor.rgb * 0.6 + vColor.rgb * 0.4 * diff;
    vec3 color = base + vec3(1.0) * 0.6 * spec;
    FragColor = vec4(color, vColor.a);
}
)glsl";


// -------------------- ECS Implementation (dense/sparse + ComponentArray) --------------------

class EntityPool {
public:
    EntityPool(){
        dense.reserve(MAX_ENTITIES);
        sparse.fill(UINT32_MAX);
        alive.fill(false);
        nextId = 0;
    }
    Entity create(){
        assert(dense.size() < MAX_ENTITIES && "max entities exceeded");
        Entity id = nextId++;
        dense.push_back(id);
        sparse[id] = static_cast<uint32_t>(dense.size()-1);
        alive[id] = true;
        return id;
    }
    void destroy(Entity e){
        assert(e < nextId);
        uint32_t idx = sparse[e];
        assert(idx != UINT32_MAX && "destroy non-existing");
        Entity last = dense.back();
        dense[idx] = last;
        sparse[last] = idx;
        dense.pop_back();
        sparse[e] = UINT32_MAX;
        alive[e] = false;
    }
    bool valid(Entity e) const {
        return e < nextId && alive[e];
    }
    const std::vector<Entity>& all() const { return dense; }
private:
    std::vector<Entity> dense;
    std::array<uint32_t, MAX_ENTITIES> sparse;
    std::array<bool, MAX_ENTITIES> alive;
    Entity nextId;
};

struct IComponentArray { virtual ~IComponentArray() = default; virtual void entityDestroyed(Entity) = 0; };

template<typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray(){
        entityToIndex.fill(npos);
    }
    void insert(Entity e, T comp){
        assert(e < MAX_ENTITIES);
        assert(entityToIndex[e] == npos && "component exists");
        size_t idx = data.size();
        data.push_back(std::move(comp));
        indexToEntity.push_back(e);
        entityToIndex[e] = idx;
    }
    void remove(Entity e){
        size_t idx = entityToIndex[e];
        assert(idx != npos && "remove non-existing");
        size_t last = data.size()-1;
        if(idx != last){
            data[idx] = std::move(data[last]);
            Entity lastE = indexToEntity[last];
            indexToEntity[idx] = lastE;
            entityToIndex[lastE] = idx;
        }
        data.pop_back();
        indexToEntity.pop_back();
        entityToIndex[e] = npos;
    }
    bool has(Entity e) const { return entityToIndex[e] != npos; }
    T& get(Entity e){ size_t idx = entityToIndex[e]; assert(idx != npos); return data[idx]; }
    const T& get(Entity e) const { size_t idx = entityToIndex[e]; assert(idx != npos); return data[idx]; }
    const std::vector<T>& rawData() const { return data; }
    const std::vector<Entity>& rawEntities() const { return indexToEntity; }
    void entityDestroyed(Entity e) override { if(has(e)) remove(e); }
    size_t size() const { return data.size(); }
private:
    static constexpr size_t npos = SIZE_MAX;
    std::vector<T> data;
    std::vector<Entity> indexToEntity;
    std::array<size_t, MAX_ENTITIES> entityToIndex;
};

class ComponentManager {
public:
    template<typename T>
    void add(Entity e, T comp){
        auto id = std::type_index(typeid(T));
        auto it = arrays.find(id);
        if(it == arrays.end()){
            auto arr = std::make_unique<ComponentArray<T>>();
            arr->insert(e, std::move(comp));
            arrays[id] = std::move(arr);
        } else {
            static_cast<ComponentArray<T>*>(it->second.get())->insert(e, std::move(comp));
        }
    }
    template<typename T>
    void remove(Entity e){
        auto id = std::type_index(typeid(T));
        auto it = arrays.find(id);
        if(it != arrays.end()) static_cast<ComponentArray<T>*>(it->second.get())->remove(e);
    }
    template<typename T>
    bool has(Entity e) const {
        auto id = std::type_index(typeid(T));
        auto it = arrays.find(id);
        if(it == arrays.end()) return false;
        return static_cast<const ComponentArray<T>*>(it->second.get())->has(e);
    }
    template<typename T>
    T& get(Entity e){
        auto id = std::type_index(typeid(T));
        auto it = arrays.find(id);
        assert(it != arrays.end());
        return static_cast<ComponentArray<T>*>(it->second.get())->get(e);
    }
    template<typename T>
    const ComponentArray<T>& getArray() const {
        auto id = std::type_index(typeid(T));
        auto it = arrays.find(id);
        assert(it != arrays.end());
        return *static_cast<const ComponentArray<T>*>(it->second.get());
    }
    void entityDestroyed(Entity e){
        for(auto &kv : arrays) kv.second->entityDestroyed(e);
    }
private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> arrays;
};

class Registry {
public:
    Entity create(){ return pool.create(); }
    void destroy(Entity e){ pool.destroy(e); comps.entityDestroyed(e); }
    template<typename T> void add(Entity e, T comp){ comps.add<T>(e, std::move(comp)); }
    template<typename T> void remove(Entity e){ comps.remove<T>(e); }
    template<typename T> bool has(Entity e) const { return comps.has<T>(e); }
    template<typename T> T& get(Entity e){ return comps.get<T>(e); }
    const std::vector<Entity>& allEntities() const { return pool.all(); }

    template<typename... Components, typename Func>
    void view(Func func){
        // choose first component's array as base to iterate for locality
        using Base = std::tuple_element_t<0, std::tuple<Components...>>;
        const auto& baseArr = comps.getArray<Base>();
        const auto& entities = baseArr.rawEntities();
        for(size_t i=0;i<entities.size();++i){
            Entity e = entities[i];
            if(!pool.valid(e)) continue;
            if((comps.has<Components>(e) && ...)){
                func(e, comps.get<Components>(e)...);
            }
        }
    }
private:
    EntityPool pool;
    ComponentManager comps;
};

// -------------------- Components --------------------
struct Transform { glm::vec3 pos{0.0f}; glm::vec3 scale{1.0f}; glm::mat4 model{1.0f}; };
struct Render   { glm::vec4 color{1.0f}; };
struct Ball     { glm::vec3 velocity{0.0f}; float radius{0.5f}; };
struct Paddle   {};
struct Brick    {};
struct Score    { int value = 0; };
struct Lives    { int value = Config::INITIAL_LIVES; };

// -------------------- Mesh helpers --------------------
struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
};

Mesh makeCubeMesh(){
    Mesh m;
    float vertices[] = {
        // a simple cube (positions only)
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 0,1,5, 5,4,0,
        2,3,7, 7,6,2, 0,3,7, 7,4,0, 1,2,6, 6,5,1
    };
    glGenVertexArrays(1,&m.vao);
    glGenBuffers(1,&m.vbo);
    glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
    m.indexCount = (GLsizei)(sizeof(indices)/sizeof(indices[0]));
    return m;
}

Mesh makeSphereMesh(int latSeg = 16, int longSeg = 16, float radius = Config::Ball::RADIUS){
    Mesh m;
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    for(int y=0;y<=latSeg;y++){
        float theta = (float)y * glm::pi<float>() / latSeg;
        float sinT = sin(theta), cosT = cos(theta);
        for(int x=0;x<=longSeg;x++){
            float phi = (float)x * 2.0f * glm::pi<float>() / longSeg;
            float sinP=sin(phi), cosP=cos(phi);
            verts.push_back(radius * cosP * sinT);
            verts.push_back(radius * cosT);
            verts.push_back(radius * sinP * sinT);
        }
    }
    for(int y=0;y<latSeg;y++){
        for(int x=0;x<longSeg;x++){
            int a = y*(longSeg+1)+x;
            int b = a + longSeg + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a+1);
            idx.push_back(b); idx.push_back(b+1); idx.push_back(a+1);
        }
    }
    glGenVertexArrays(1,&m.vao);
    glGenBuffers(1,&m.vbo);
    glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
    m.indexCount = (GLsizei)idx.size();
    return m;
}

// -------------------- Game class integrating ECS + GL --------------------

class Game {
public:
    Game() = default;
    ~Game(){ shutdown(); }

    // -------------------- GL helper classes --------------------
    struct GLBuffer {
        GLuint id = 0;
        GLenum target = GL_ARRAY_BUFFER;
        void* mappedPtr = nullptr;
        size_t size = 0;

        void init(GLenum t = GL_ARRAY_BUFFER){
            target = t;
            glGenBuffers(1, &id);
        }
        ~GLBuffer(){
            if(mappedPtr) glUnmapBuffer(target);
            if(id) glDeleteBuffers(1,&id);
        }
        void allocate(size_t sz){
            size = sz;
            bind();
            glBufferStorage(target, sz, nullptr,
                            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
            mappedPtr = glMapBufferRange(target, 0, sz,
                                         GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
            if(!mappedPtr) std::cerr<<"Persistent mapping failed\n";
        }
        void bind() const { glBindBuffer(target, id); }
        void setSubData(const void* data, size_t sz, size_t offset=0){
            memcpy((char*)mappedPtr + offset, data, sz);
        }
    };


    struct UBO {
        GLuint id = 0;
        UBO() = default;
        void init(){
            glGenBuffers(1,&id);
        }
        ~UBO(){ if(id) glDeleteBuffers(1,&id); }
        void bind() const { glBindBuffer(GL_UNIFORM_BUFFER, id); }
        void allocate(size_t s){ bind(); glBufferData(GL_UNIFORM_BUFFER, s, nullptr, GL_DYNAMIC_DRAW); }
        void setSubData(const void* data, size_t s, size_t offset=0){ bind(); glBufferSubData(GL_UNIFORM_BUFFER, offset, s, data); }
    };

    // -------------------- PhysicsSystem --------------------
    struct PhysicsSystem {
        Registry* reg = nullptr;                  // pointer na registry
        std::vector<Entity> toDestroy;
        bool* runningPtr = nullptr;               // pointer na Game::running

        PhysicsSystem(Registry* r = nullptr, bool* run = nullptr)
            : reg(r), runningPtr(run) { toDestroy.reserve(32); }

        void setRunningPtr(bool* run){ runningPtr = run; }

        void reset(){
            if(!reg) return;
            reg->view<Paddle, Transform>([](Entity, Paddle&, Transform& t){
                t.pos = Config::Paddle::START_POS;
                t.model = glm::scale(glm::translate(glm::mat4(1.0f), t.pos), t.scale);
            });
            reg->view<Ball, Transform>([](Entity, Ball& b, Transform& t){
                t.pos = Config::Ball::START_POS;
                b.velocity = Config::Ball::START_VEL;
                t.model = glm::scale(glm::translate(glm::mat4(1.0f), t.pos), t.scale);
            });
        }

        bool update(float dt){
            if(!reg) return true;

            reg->view<Ball, Transform, Ball>([&](Entity e, Ball& b, Transform& t, Ball& unused){
                t.pos += b.velocity * dt;

                // --- odraz od stěn ---
                if(t.pos.x - b.radius < Config::World::MIN_X || t.pos.x + b.radius > Config::World::MAX_X){
                    b.velocity.x *= -1.0f;
                    t.pos.x = glm::clamp(t.pos.x, Config::World::MIN_X + b.radius, Config::World::MAX_X - b.radius);
                }
                if(t.pos.y + b.radius > Config::World::MAX_Y){
                    b.velocity.y *= -1.0f;
                    t.pos.y = Config::World::MAX_Y - b.radius;
                }

                reg->view<Paddle, Transform>([&](Entity, Paddle&, Transform& paddle){
                    if(aabbCollision2D(paddle.pos, paddle.scale, t.pos, b.radius)){
                        b.velocity.y = fabs(b.velocity.y);
                        t.pos.y = paddle.pos.y + paddle.scale.y*0.5f + b.radius;

                        // --- zrychlení míče ---
                        b.velocity *= Config::Ball::SPEEDUP_FACTOR;
                        if(glm::length(b.velocity) > Config::Ball::MAX_SPEED)
                            b.velocity = glm::normalize(b.velocity) * Config::Ball::MAX_SPEED;
                    }
                });

                // --- odraz od bricks ---
                std::vector<Entity> destroyedBricks;
                reg->view<Brick, Transform>([&](Entity brick, Brick&, Transform& brickT){
                    if(aabbCollision2D(brickT.pos, brickT.scale, t.pos, b.radius)){
                        destroyedBricks.push_back(brick);

                        // invert Y
                        b.velocity.y *= -1.0f;

                        // --- zrychlení míče ---
                        b.velocity *= Config::Ball::SPEEDUP_FACTOR;
                        if(glm::length(b.velocity) > Config::Ball::MAX_SPEED)
                            b.velocity = glm::normalize(b.velocity) * Config::Ball::MAX_SPEED;
                    }
                });


                for(Entity brick : destroyedBricks){
                    reg->destroy(brick);
                    reg->view<Score>([&](Entity, Score& s){ s.value += Config::SCORE_PER_BRICK; });
                }

                // --- smrt (míček pod spodní hranou) ---
                if(t.pos.y - b.radius < Config::World::MIN_Y){
                    reg->view<Lives>([&](Entity, Lives& l){
                        l.value -= 1;
                        if(l.value <= 0){
                            // konec hry
                            if(runningPtr) *runningPtr = false;
                        }
                    });
                    reset();
                }

                // update model matrix
                t.model = glm::scale(glm::translate(glm::mat4(1.0f), t.pos), t.scale);
            });

            return runningPtr ? *runningPtr : true;
        }

        static bool aabbCollision2D(const glm::vec3& boxPos,const glm::vec3& boxScale,
                                    const glm::vec3& ballPos,float radius){
            return (ballPos.x + radius > boxPos.x - boxScale.x*0.5f &&
                    ballPos.x - radius < boxPos.x + boxScale.x*0.5f &&
                    ballPos.y + radius > boxPos.y - boxScale.y*0.5f &&
                    ballPos.y - radius < boxPos.y + boxScale.y*0.5f);
        }
    };

    // -------------------- Členové Game --------------------
    GLFWwindow* window = nullptr;
    Registry registry;
    PhysicsSystem physics;  // pointer na registry přiřadíme v init

    GLuint program = 0;
    Mesh cubeMesh{}, sphereMesh{};
    GLBuffer instanceVBO, colorVBO;
    UBO cameraUBO;

    glm::mat4 view{1.0f}, projection{1.0f};
    bool running = true;
    bool restartRequested = false;
    bool popupOpened = false;
    Stats stats;
    float fpsTimer = 0.0f;
    int frameCount = 0;

    // -------------------- Inicializace --------------------
    bool init(){
        if(!glfwInit()) return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Modern ECS Arkanoid", nullptr, nullptr);
        if(!window){ glfwTerminate(); return false; }
        glfwMakeContextCurrent(window);
        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"GLAD init failed\n"; return false; }
        glEnable(GL_DEPTH_TEST);
        glfwSwapInterval(0); // VSync off for benchmarking
        // ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        // -------------------- GL Buffers init --------------------
        instanceVBO.init();
        colorVBO.init();
        cameraUBO.init();

        // Compile shader
        compileShader();

        // Meshes
        cubeMesh = makeCubeMesh();
        sphereMesh = makeSphereMesh(16,16, Config::Ball::RADIUS);

        // Camera UBO
        cameraUBO.bind();
        cameraUBO.allocate(2 * sizeof(glm::mat4));
        glBindBufferBase(GL_UNIFORM_BUFFER, CAMERA_UBO_BINDING, cameraUBO.id);

        // Instance buffers
        size_t maxInstances = Config::Bricks::COLS * Config::Bricks::ROWS + 2;
        instanceVBO.bind();
        instanceVBO.allocate(maxInstances * sizeof(glm::mat4));
        colorVBO.bind();
        colorVBO.allocate(maxInstances * sizeof(glm::vec4));

        setupInstancedAttributesForMesh(cubeMesh.vao);
        setupInstancedAttributesForMesh(sphereMesh.vao);

        // -------------------- ECS --------------------
        physics = PhysicsSystem(&registry, &running); // předáme pointer na Game::running

        // přiřazujeme pointer na registry
        setupGame();

        // Camera
        view = glm::lookAt(CAMERA_POS,
                           CAMERA_POS + CAMERA_FRONT,
                           CAMERA_UP);
        projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);

        return true;
    }

    void run(){
        float last = (float)glfwGetTime();
        glfwSwapInterval(0); // VSync off for benchmarking
        while(!glfwWindowShouldClose(window)){
            float now = (float)glfwGetTime();
            float dt = glm::clamp(now - last, 0.0f, 0.033f);
            last = now;

            glfwPollEvents();
            input(dt);

            if(running){
                running = physics.update(dt);
            }

            render();
            gui();

            if(restartRequested){
                setupGame();
                running = true;
                restartRequested = false;
                popupOpened = false;
            }

            glfwSwapBuffers(window);
            frameCount++;
            fpsTimer += dt;
            if (fpsTimer >= 1.0f) {
                std::cout << "FPS: " << frameCount << " | " << std::endl;
                fpsTimer = 0; frameCount = 0;
            }
            stats.update(dt);
        }
    }

private:
    // ----- GL helper classes -----
    // convenience wrapper
    void compileShader(){
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertexSrc, NULL);
        glCompileShader(vs);
        checkShaderCompile(vs, "vertex");
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragmentSrc, NULL);
        glCompileShader(fs);
        checkShaderCompile(fs, "fragment");
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        checkProgramLink(program, "program");
        glDeleteShader(vs); glDeleteShader(fs);
        // bind UBO index to binding point
        GLuint blockIndex = glGetUniformBlockIndex(program, "Camera");
        if(blockIndex != GL_INVALID_INDEX) glUniformBlockBinding(program, blockIndex, CAMERA_UBO_BINDING);
    }

    void setupInstancedAttributesForMesh(GLuint vao){
        glBindVertexArray(vao);
        // bind instanceVBO to attribute locations 1..4 (mat4)
        instanceVBO.bind();
        GLsizei vec4Size = sizeof(glm::vec4);
        for(unsigned int i=0;i<4;++i){
            GLuint loc = 1 + i;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
            glVertexAttribDivisor(loc, 1);
        }
        // color at location 5
        colorVBO.bind();
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
        glVertexAttribDivisor(5, 1);
        glBindVertexArray(0);
    }

    // ----- Game setup -----
    void setupGame(){
        // clear registry by creating a fresh one (simple approach)
        registry = Registry(); // note: rebuilds new Registry
        // create paddle
        Entity pid = registry.create();
        Transform pt; pt.pos = Config::Paddle::START_POS; pt.scale = Config::Paddle::SCALE; pt.model = glm::scale(glm::translate(glm::mat4(1.0f), pt.pos), pt.scale);
        registry.add<Transform>(pid, pt);
        registry.add<Render>(pid, Render{glm::vec4(0.3f,0.8f,0.3f,1.0f)});
        registry.add<Paddle>(pid, Paddle{});

        // bricks
        for(int r=0; r<Config::Bricks::ROWS; ++r){
            for(int c=0; c<Config::Bricks::COLS; ++c){
                Entity b = registry.create();
                float px = Config::Bricks::START_X + c * Config::Bricks::SCALE.x;
                float py = Config::Bricks::START_Y - r * (Config::Bricks::SCALE.y + Config::Bricks::SPACING_Y);
                Transform bt;
                bt.pos = glm::vec3(px, py, 0.0f);
                bt.scale = Config::Bricks::SCALE;
                bt.model = glm::scale(glm::translate(glm::mat4(1.0f), bt.pos), bt.scale);
                registry.add<Transform>(b, bt);
                registry.add<Render>(b, Render{ randColor() });
                registry.add<Brick>(b, Brick{});
            }
        }

        // ball
        Entity bid = registry.create();
        Transform bt; bt.pos = Config::Ball::START_POS; bt.scale = glm::vec3(Config::Ball::RADIUS); bt.model = glm::scale(glm::translate(glm::mat4(1.0f), bt.pos), bt.scale);
        registry.add<Transform>(bid, bt);
        registry.add<Render>(bid, Render{ glm::vec4(1.0f,0.2f,0.2f,1.0f) });
        registry.add<Ball>(bid, Ball{ Config::Ball::START_VEL, Config::Ball::RADIUS });

        // player score/lives
        Entity player = registry.create();
        registry.add<Score>(player, Score{0});
        registry.add<Lives>(player, Lives{Config::INITIAL_LIVES});

        // reset physics internal caches
        physics.reset();    // resetuje pozice paddle a míčku
        running = true;     // nastavíme, že hra běží
        restartRequested = false;
    }

    // ----- Input -----
    void input(float dt){
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        // move paddle by mouse X
        double mx, my; glfwGetCursorPos(window,&mx,&my);
        int w,h; glfwGetWindowSize(window,&w,&h);
        float worldX = static_cast<float>(mx) / (float)w * (Config::World::MAX_X - Config::World::MIN_X) + Config::World::MIN_X;
        // get paddle transform
        registry.view<Paddle, Transform>([&](Entity e, Paddle&, Transform& t){
            float smooth = 15.0f;
            t.pos.x += (worldX - t.pos.x) * smooth * dt;
            float halfW = t.scale.x * 0.5f;
            t.pos.x = std::clamp(t.pos.x, Config::World::MIN_X + halfW, Config::World::MAX_X - halfW);
            t.model = glm::scale(glm::translate(glm::mat4(1.0f), t.pos), t.scale);
        });
    }

    // ----- Render -----
    void render(){
        // update camera UBO
        cameraUBO.bind();
        cameraUBO.setSubData(glm::value_ptr(view), sizeof(glm::mat4), 0);
        cameraUBO.setSubData(glm::value_ptr(projection), sizeof(glm::mat4), sizeof(glm::mat4));

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        // Build instance arrays: bricks first, then paddle, then ball
        std::vector<glm::mat4> models;
        std::vector<glm::vec4> colors;
        models.reserve(Config::Bricks::ROWS * Config::Bricks::COLS + 2);
        colors.reserve(models.size());

        registry.view<Brick, Transform, Render>([&](Entity e, Brick&, Transform& t, Render& r){
            models.push_back(t.model);
            colors.push_back(r.color);
        });
        registry.view<Paddle, Transform, Render>([&](Entity e, Paddle&, Transform& t, Render& r){
            models.push_back(t.model);
            colors.push_back(r.color);
        });
        registry.view<Ball, Transform, Render>([&](Entity e, Ball&, Transform& t, Render& r){
            models.push_back(t.model);
            colors.push_back(r.color);
        });

        GLsizei instanceCount = (GLsizei)models.size();
        if(instanceCount > 0){
            // upload instance matrices
            instanceVBO.bind();
            // map + memcpy for speed
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO.id);
            void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::mat4),
                                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if(ptr){
                memcpy(ptr, models.data(), instanceCount * sizeof(glm::mat4));
                glUnmapBuffer(GL_ARRAY_BUFFER);
            } else {
                // fallback
                instanceVBO.setSubData(models.data(), instanceCount * sizeof(glm::mat4), 0);
            }
            // upload colors
            colorVBO.bind();
            glBindBuffer(GL_ARRAY_BUFFER, colorVBO.id);
            void* ptrc = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec4),
                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if(ptrc){
                memcpy(ptrc, colors.data(), instanceCount * sizeof(glm::vec4));
                glUnmapBuffer(GL_ARRAY_BUFFER);
            } else {
                colorVBO.setSubData(colors.data(), instanceCount * sizeof(glm::vec4), 0);
            }

            // Draw cubes (bricks + paddle)
            glBindVertexArray(cubeMesh.vao);
            // draw first Nbricks+1 maybe, but cube mesh instanceCount includes bricks+paddle
            glDrawElementsInstanced(GL_TRIANGLES, cubeMesh.indexCount, GL_UNSIGNED_INT, 0, instanceCount - 1); // assume last is ball (sphere) -> adjust below
            glBindVertexArray(0);

            // Ball is drawn with sphere mesh (we put its model/color at end)
            // We'll draw last instance as sphere (safe because we appended ball last)
            if(instanceCount >= 1){
                // sphere uses same instance buffers (attributes already set for sphere VAO)
                // draw only last 1 instance: we can draw sphere with instanceCount=1, but need base instance offset
                // Since we uploaded all instance data sequentially, and attributes are per-instance not using baseInstance,
                // easiest is to upload only sphere data separately OR draw sphere by setting instance pointer offset.
                // We'll draw sphere by setting vertex attrib pointer offset via binding buffer and glVertexAttribPointer with offset.
                // Simpler: upload ball's model/color into GPU at position 0 of instance buffer and draw sphere instance 1,
                // but that would require additional copies. For simplicity: draw sphere as single non-instanced mesh using its model/color:
                glm::mat4 ballModel = models.back();
                glm::vec4 ballColor = colors.back();

                // Temporarily set a small shader uniform override: but shader expects per-vertex attributes.
                // Easiest approach: create a temporary single-instance upload at offset 0 and draw sphere with instanceCount=1.
                // Save existing first matrix/color
                glm::mat4 savedMat;
                glm::vec4 savedColor;
                if(instanceCount >= 1){
                    // read first matrix at GPU? too complex - easier path: allocate a small temporary buffer and set vertex attrib pointers for sphere to read from that buffer.
                    // We'll set a temporary VBO with ball model/color and set divisors = 1 and draw one instance.
                    GLuint tmpBuf, tmpColorBuf;
                    glGenBuffers(1,&tmpBuf);
                    glGenBuffers(1,&tmpColorBuf);
                    glBindBuffer(GL_ARRAY_BUFFER, tmpBuf);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4), &ballModel, GL_DYNAMIC_DRAW);
                    glBindBuffer(GL_ARRAY_BUFFER, tmpColorBuf);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4), &ballColor, GL_DYNAMIC_DRAW);

                    glBindVertexArray(sphereMesh.vao);
                    GLsizei vec4Size = sizeof(glm::vec4);
                    // set attribs 1..4 to tmpBuf
                    glBindBuffer(GL_ARRAY_BUFFER, tmpBuf);
                    for(unsigned int i=0;i<4;++i){
                        GLuint loc = 1 + i;
                        glEnableVertexAttribArray(loc);
                        glVertexAttribPointer(loc,4,GL_FLOAT,GL_FALSE,sizeof(glm::mat4),(void*)(i*vec4Size));
                        glVertexAttribDivisor(loc,1);
                    }
                    glBindBuffer(GL_ARRAY_BUFFER, tmpColorBuf);
                    glEnableVertexAttribArray(5);
                    glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(glm::vec4),(void*)0);
                    glVertexAttribDivisor(5,1);

                    glDrawElementsInstanced(GL_TRIANGLES, sphereMesh.indexCount, GL_UNSIGNED_INT, 0, 1);

                    // cleanup
                    glBindVertexArray(0);
                    glDeleteBuffers(1,&tmpBuf);
                    glDeleteBuffers(1,&tmpColorBuf);
                }
            }
        }

        glUseProgram(0);
    }

    // ----- GUI -----
    void gui(){
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10,10));
        ImGui::Begin("Game Info", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        registry.view<Score>([](Entity e, Score& s){ ImGui::TextColored(ImVec4(1,1,0,1), "Score: %d", s.value); });
        registry.view<Lives>([](Entity e, Lives& l){ ImGui::TextColored(ImVec4(1,0,0,1), "Lives: %d", l.value); });
        ImGui::End();
        stats.drawUI();
        if(!running){
            if(!popupOpened){ ImGui::OpenPopup("Game Over!"); popupOpened = true; }
        }

        if(ImGui::BeginPopupModal("Game Over!", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("YOU LOST ALL YOUR LIVES!\n");
            ImGui::Separator();
            if(ImGui::Button("Restart")){ restartRequested = true; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if(ImGui::Button("Quit")){ glfwSetWindowShouldClose(window, true); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void shutdown(){
        if(window){
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            glfwDestroyWindow(window);
            glfwTerminate();
            window = nullptr;
        }
        if(program) { glDeleteProgram(program); program = 0; }
    }


};

// -------------------- main --------------------
int main(){
    Game game;
    if(!game.init()){
        std::cerr<<"Initialization failed\n";
        return -1;
    }
    game.run();
    return 0;
}

