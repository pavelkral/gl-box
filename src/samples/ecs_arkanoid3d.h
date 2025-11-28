// main.cpp
// 3D ECS Arkanoid - ECS-friendly instancing for bricks
// Modern OpenGL (VAO/VBO/EBO), UBO for camera, ImGui UI (score/lives + restart modal)
// Single-file example for learning / prototype use.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

// -------------------- Constants --------------------
namespace Constants {
static constexpr unsigned int SCR_WIDTH  = 1280u;
static constexpr unsigned int SCR_HEIGHT = 720u;
// world bounds
static constexpr float WORLD_MIN_X = -15.0f;
static constexpr float WORLD_MAX_X =  15.0f;
static constexpr float WORLD_MIN_Y = -10.0f;
static constexpr float WORLD_MAX_Y =  15.0f;
// bricks grid
static constexpr int BRICK_ROWS    = 5;
static constexpr int BRICK_COLS    = 10;
static constexpr float BRICK_SPACING_X = 3.0f;
static constexpr float BRICK_SPACING_Y = 2.0f;
static constexpr float BRICK_OFFSET_X = -13.5f;
static constexpr float BRICK_OFFSET_Y = 2.0f;
static constexpr glm::vec3 BRICK_SCALE = {2.5f, 1.0f, 1.0f};
// paddle
static constexpr glm::vec3 PADDLE_START_POS = {0.0f, -5.0f, 0.0f};
static constexpr glm::vec3 PADDLE_SCALE = {6.0f, 1.0f, 2.0f};
static constexpr float PADDLE_SPEED = 20.0f;
// ball
static constexpr glm::vec3 BALL_START_POS = {0.0f, -3.0f, 0.0f};
static constexpr glm::vec3 BALL_START_VEL = {5.0f, 8.0f, 0.0f};
static constexpr float BALL_RADIUS = 1.0f;
// camera
static constexpr glm::vec3 CAMERA_POS   = {0.0f, 10.0f, 35.0f};
static constexpr glm::vec3 CAMERA_FRONT = {0.0f, -0.1f, -1.0f};
static constexpr glm::vec3 CAMERA_UP    = {0.0f, 1.0f, 0.0f};
static constexpr GLuint CAMERA_UBO_BINDING = 0;
// gameplay
static constexpr int INITIAL_LIVES = 3;
static constexpr int SCORE_PER_BRICK = 10;
}

//
// -------------------- Utility Helpers --------------------
//
static void checkShaderCompile(GLuint shader, const char* name) {
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(shader, 1024, NULL, buf);
        std::cerr << "[Shader compile error] " << name << ":\n" << buf << "\n";
    }
}
static void checkProgramLink(GLuint prog, const char* name) {
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, 1024, NULL, buf);
        std::cerr << "[Program link error] " << name << ":\n" << buf << "\n";
    }
}

//
// -------------------- ECS definitions --------------------
//
using EntityID = unsigned int;

struct TransformComponent {
    glm::vec3 pos{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotationAxis{0.0f,1.0f,0.0f};
    float angle = 0.0f;
};

struct RenderComponent {
    GLuint vao = 0;
    GLsizei indexCount = 0;
    glm::vec4 color{1.0f};
};

struct BallComponent {
    glm::vec3 velocity;
    float radius;
};

struct PaddleComponent {};
struct BrickComponent {};
struct ScoreComponent {
    int value = 0;
};
struct LivesComponent {
    int value = Constants::INITIAL_LIVES;
};

class EntityManager {
public:
    std::unordered_map<EntityID, TransformComponent> transforms;
    std::unordered_map<EntityID, RenderComponent> renders;
    std::unordered_map<EntityID, BallComponent> balls;
    std::unordered_map<EntityID, PaddleComponent> paddles;
    std::unordered_map<EntityID, BrickComponent> bricks;
    std::unordered_map<EntityID, ScoreComponent> scores;
    std::unordered_map<EntityID, LivesComponent> lives;

    std::vector<EntityID> entities;            // all entity ids (for iteration order)
    std::vector<EntityID> brickOrder;          // brick ids in a stable order for instancing
    EntityID nextID = 0;

    EntityID createEntity() {
        EntityID id = nextID++;
        entities.push_back(id);
        transforms[id] = TransformComponent{};
        renders[id] = RenderComponent{};
        return id;
    }

    void destroyEntity(EntityID id) {
        // remove from entities vector
        entities.erase(std::remove(entities.begin(), entities.end(), id), entities.end());
        // remove from brickOrder if present
        brickOrder.erase(std::remove(brickOrder.begin(), brickOrder.end(), id), brickOrder.end());
        // erase component maps
        transforms.erase(id);
        renders.erase(id);
        balls.erase(id);
        paddles.erase(id);
        bricks.erase(id);
        scores.erase(id);
        lives.erase(id);
    }

    void reset() {
        entities.clear();
        brickOrder.clear();
        transforms.clear();
        renders.clear();
        balls.clear();
        paddles.clear();
        bricks.clear();
        scores.clear();
        lives.clear();
        nextID = 0;
    }
};

//
// -------------------- Mesh creation --------------------
// Minimal cube (position only) and sphere (position only).
//
struct Mesh {
    GLuint vao=0,vbo=0, ebo=0;
    GLsizei indexCount = 0;
};

Mesh makeCube() {
    Mesh m;
    // 8 verts, 36 indices
    float vertices[] = {
        -0.5f,-0.5f,-0.5f,
        0.5f,-0.5f,-0.5f,
        0.5f, 0.5f,-0.5f,
        -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,
        0.5f,-0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,1,5, 5,4,0,
        2,3,7, 7,6,2,
        0,3,7, 7,4,0,
        1,2,6, 6,5,1
    };
    m.indexCount = 36;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    glBindVertexArray(0);
    return m;
}

// Sphere generator (indexed)
Mesh makeSphere(int latSeg = 16, int longSeg = 16, float radius = 0.5f) {
    Mesh m;
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    verts.reserve((latSeg+1)*(longSeg+1)*3);
    for (int y=0;y<=latSeg;y++){
        float theta = (float)y * glm::pi<float>() / latSeg;
        float sinT = sin(theta), cosT = cos(theta);
        for (int x=0;x<=longSeg;x++){
            float phi = (float)x * 2.0f * glm::pi<float>() / longSeg;
            float sinP = sin(phi), cosP = cos(phi);
            verts.push_back(radius * cosP * sinT);
            verts.push_back(radius * cosT);
            verts.push_back(radius * sinP * sinT);
        }
    }
    for (int y=0;y<latSeg;y++){
        for (int x=0;x<longSeg;x++){
            int a = y*(longSeg+1)+x;
            int b = a+longSeg+1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a+1);
            idx.push_back(b); idx.push_back(b+1); idx.push_back(a+1);
        }
    }
    m.indexCount = (GLsizei)idx.size();
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    glBindVertexArray(0);
    return m;
}

const char* vertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

// instance model matrix (mat4 takes 4 attribute slots)

layout(location = 1) in vec4 aModelRow0;
layout(location = 2) in vec4 aModelRow1;
layout(location = 3) in vec4 aModelRow2;
layout(location = 4) in vec4 aModelRow3;
layout(location = 5) in vec4 aColor;

layout(std140) uniform Camera {
    mat4 view;
    mat4 projection;
};

out vec4 vColor;
out vec3 vWorldPos;
out vec3 Normal;

void main(){
    mat4 model = mat4(aModelRow0, aModelRow1, aModelRow2, aModelRow3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vColor = aColor;
    Normal = normalize(mat3(model) * aPos);
    gl_Position = projection * view * worldPos;
}
)glsl";



const char* fragmentShaderSrc = R"glsl(
#version 330 core

in vec3 vWorldPos;
in vec3 Normal;
in vec4 vColor;
out vec4 FragColor;

void main() {
    vec3 N = normalize(Normal);
    vec3 lightPos = vec3(10.0, 20.0, 10.0);
    vec3 L = normalize(lightPos - vWorldPos);
    vec3 V = normalize(vec3(0.0, 15.0, 35.0) - vWorldPos); // camera pos baked
    // simple Blinn-Phong
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 base = vColor.rgb * 0.6 + vColor.rgb * 0.4 * diff;
    vec3 color = base + vec3(1.0) * 0.6 * spec; // add highlight
    FragColor = vec4(color, vColor.a);
}
)glsl";
//
// -------------------- Rendering / ECS Systems --------------------
//

struct GLResources {
    // main shader
    GLuint program = 0;
    GLuint cameraUBO = 0;
    Mesh cubeMesh;
    Mesh sphereMesh;
    GLuint instanceVBO = 0; // model matrices
    GLuint colorVBO = 0;    // per-instance colors
    // VAO configured for instanced drawing (shares cube mesh VBO/EBO)
    GLuint instancedVAO = 0;
};

static GLResources glr;

static void initGLResources() {
    // compile shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSrc, NULL);
    glCompileShader(vs);
    checkShaderCompile(vs, "vertex");

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSrc, NULL);
    glCompileShader(fs);
    checkShaderCompile(fs, "fragment");

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    checkProgramLink(prog, "program");

    glDeleteShader(vs);
    glDeleteShader(fs);

    glr.program = prog;

    // create UBO for camera (view + projection) - std140 alignment mat4 is 16-byte aligned
    glGenBuffers(1, &glr.cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, glr.cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, Constants::CAMERA_UBO_BINDING, glr.cameraUBO);

    // associate the uniform block index with the binding point
    GLuint blockIndex = glGetUniformBlockIndex(glr.program, "Camera");
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(glr.program, blockIndex, Constants::CAMERA_UBO_BINDING);
    }

    // create meshes
    glr.cubeMesh = makeCube();
    glr.sphereMesh = makeSphere(16, 16, Constants::BALL_RADIUS);

    // create instancing buffers (empty initially)
    glGenBuffers(1, &glr.instanceVBO);
    glGenBuffers(1, &glr.colorVBO);

    // prepare instanced VAO: reuse cubeMesh VBO/EBO but set instance attribs
    glGenVertexArrays(1, &glr.instancedVAO);
    glBindVertexArray(glr.instancedVAO);

    // bind base vertex buffer and index buffer from cube mesh
    glBindBuffer(GL_ARRAY_BUFFER, glr.cubeMesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glr.cubeMesh.ebo);
    // position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    // instance model matrix uses locations 1..4 (each vec4)
    glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
    // allocate a small initial size - will update dynamically
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * 32, NULL, GL_DYNAMIC_DRAW);
    GLsizei vec4Size = sizeof(glm::vec4);
    for (unsigned int i = 0; i < 4; ++i) {
        GLuint loc = 1 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
        glVertexAttribDivisor(loc, 1);
    }

    // per-instance color at location 5
    glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 32, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    // leave buffers bound to their objects
}

static void cleanupGLResources() {
    if (glr.instanceVBO) glDeleteBuffers(1, &glr.instanceVBO);
    if (glr.colorVBO)    glDeleteBuffers(1, &glr.colorVBO);
    if (glr.cameraUBO)   glDeleteBuffers(1, &glr.cameraUBO);
    if (glr.instancedVAO) glDeleteVertexArrays(1, &glr.instancedVAO);

    if (glr.cubeMesh.vbo) glDeleteBuffers(1, &glr.cubeMesh.vbo);
    if (glr.cubeMesh.ebo) glDeleteBuffers(1, &glr.cubeMesh.ebo);
    if (glr.cubeMesh.vao) glDeleteVertexArrays(1, &glr.cubeMesh.vao);

    if (glr.sphereMesh.vbo) glDeleteBuffers(1, &glr.sphereMesh.vbo);
    if (glr.sphereMesh.ebo) glDeleteBuffers(1, &glr.sphereMesh.ebo);
    if (glr.sphereMesh.vao) glDeleteVertexArrays(1, &glr.sphereMesh.vao);

    if (glr.program) glDeleteProgram(glr.program);
}

//
// -------------------- Game setup / helper --------------------
//
static void updateCameraUBO(const glm::mat4& view, const glm::mat4& proj) {
    glBindBuffer(GL_UNIFORM_BUFFER, glr.cameraUBO);
    // write view then proj (std140 / mat4)
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static void buildInstancingBuffersForBricks(EntityManager& em) {
    // build model matrix array and color array in order of em.brickOrder
    size_t n = em.brickOrder.size();
    std::vector<glm::mat4> models;
    models.reserve(n);
    std::vector<glm::vec4> colors;
    colors.reserve(n);

    for (EntityID id : em.brickOrder) {
        auto &t = em.transforms[id];
        auto &r = em.renders[id];
        glm::mat4 model(1.0f);
        model = glm::translate(model, t.pos);
        model = glm::rotate(model, t.angle, t.rotationAxis);
        model = glm::scale(model, t.scale);
        models.push_back(model);
        colors.push_back(r.color);
    }

    // update instance VBO and color VBO (use glBufferData for resize, glBufferSubData otherwise)
    glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, models.size() * sizeof(glm::mat4), models.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), colors.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//
// -------------------- Systems --------------------
//

void InputSystem(EntityManager& em, GLFWwindow* window, float dt) {
    // mouse X converted to world X in [-15,15]
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int w,h; glfwGetWindowSize(window, &w, &h);
    float worldX = static_cast<float>(mx) / float(w) * (Constants::WORLD_MAX_X - Constants::WORLD_MIN_X) + Constants::WORLD_MIN_X;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){

            glfwSetWindowShouldClose(window, true);

    }

    for (auto &kv : em.paddles) {
        EntityID pid = kv.first;
        auto &t = em.transforms[pid];

        // keyboard
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) t.pos.x -= Constants::PADDLE_SPEED * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) t.pos.x += Constants::PADDLE_SPEED * dt;

        // mouse: smooth follow
        float smooth = 10.0f;
        t.pos.x += (worldX - t.pos.x) * smooth * dt;

        // clamp within world X bounds
        float halfW = t.scale.x * 0.5f;
        if (t.pos.x - halfW < Constants::WORLD_MIN_X) t.pos.x = Constants::WORLD_MIN_X + halfW;
        if (t.pos.x + halfW > Constants::WORLD_MAX_X) t.pos.x = Constants::WORLD_MAX_X - halfW;
    }
}

class PhysicsSystem {
public:
    EntityManager &em;
    glm::vec3 paddleStart;
    glm::vec3 ballStart;
    glm::vec3 ballVelStart;
    std::vector<EntityID> destroyed;

    PhysicsSystem(EntityManager& e)
        : em(e), paddleStart(Constants::PADDLE_START_POS),
        ballStart(Constants::BALL_START_POS),
        ballVelStart(Constants::BALL_START_VEL)
    {
        destroyed.reserve(8);
    }
    void reset(const glm::vec3& paddlePos,
               const glm::vec3& ballPos,
               const glm::vec3& ballVel)
    {
        paddleStart = paddlePos;
        ballStart = ballPos;
        ballVelStart = ballVel;
    }
    // returns true if game continues, false -> game over (lives exhausted)
    bool update(float dt) {
        bool needReset = false;
        destroyed.clear();
        // iterate over balls (normally one)
        for (auto &kv : em.balls) {
            EntityID bid = kv.first;
            auto &ballT = em.transforms[bid];
            auto &ballC = em.balls[bid];

            // move
            ballT.pos += ballC.velocity * dt;
            ballT.pos.z = 0.0f; // fixed 2D plane

            // walls
            if (ballT.pos.x - ballC.radius < Constants::WORLD_MIN_X) {
                ballT.pos.x = Constants::WORLD_MIN_X + ballC.radius;
                ballC.velocity.x *= -1.0f;
                std::cout << "Ball hit left wall\n";
            }
            if (ballT.pos.x + ballC.radius > Constants::WORLD_MAX_X) {
                ballT.pos.x = Constants::WORLD_MAX_X - ballC.radius;
                ballC.velocity.x *= -1.0f;
                std::cout << "Ball hit right wall\n";
            }
            if (ballT.pos.y + ballC.radius > Constants::WORLD_MAX_Y) {
                ballT.pos.y = Constants::WORLD_MAX_Y - ballC.radius;
                ballC.velocity.y *= -1.0f;
                std::cout << "Ball hit top wall\n";
            }

            // paddle collisions
            for (auto &pp : em.paddles) {
                EntityID pid = pp.first;
                auto &pT = em.transforms[pid];
                if (aabbCollision2D(pT.pos, pT.scale, ballT.pos, ballC.radius)) {
                    float rel = (ballT.pos.x - pT.pos.x) / (pT.scale.x * 0.5f);
                    rel = glm::clamp(rel, -1.0f, 1.0f);
                    float speed = glm::length(glm::vec2(ballC.velocity.x, ballC.velocity.y));
                    float angle = rel * glm::radians(45.0f);
                    ballC.velocity.x = speed * sin(angle);
                    ballC.velocity.y = speed * cos(angle);
                    ballT.pos.y = pT.pos.y + pT.scale.y * 0.5f + ballC.radius;
                    std::cout << "Ball hit paddle\n";
                }
            }

            // brick collisions (collect destructions, update score)

            for (auto &bb : em.bricks) {
                EntityID brickID = bb.first;
                auto &bT = em.transforms[brickID];
                if (aabbCollision2D(bT.pos, bT.scale, ballT.pos, ballC.radius)) {
                    glm::vec2 dir = glm::vec2(ballT.pos.x - bT.pos.x, ballT.pos.y - bT.pos.y);
                    if (std::abs(dir.x) > std::abs(dir.y)) ballC.velocity.x *= -1.0f;
                    else ballC.velocity.y *= -1.0f;
                    destroyed.push_back(brickID);
                }
            }
            if (!destroyed.empty()) {
                for (EntityID b : destroyed) {
                    em.destroyEntity(b);
                    std::cout << "Ball hit brick " << b << "\n";
                    for (auto &sd : em.scores) {
                        sd.second.value += Constants::SCORE_PER_BRICK;
                        std::cout << "Score: " << sd.second.value << "\n";
                    }
                }
                // rebuilt instancing order/buffers will be done in render step
            }

            // fell below world
            if (ballT.pos.y - ballC.radius < Constants::WORLD_MIN_Y) {
                for (auto &lv : em.lives) {
                    lv.second.value--;
                    std::cout << "Life lost! Lives left: " << lv.second.value << "\n";
                }
                needReset = true;
            }
        }

        if (needReset) {
            // reset ball+paddle positions (preserve lives/score)
            for (auto &pp : em.paddles) {
                em.transforms[pp.first].pos = paddleStart;
            }
            for (auto &bb : em.balls) {
                em.transforms[bb.first].pos = ballStart;
                em.balls[bb.first].velocity = ballVelStart;
            }
        }

        // game over if any lives <= 0 (supports single player)
        for (auto &lv : em.lives) {
            if (lv.second.value <= 0) return false;
        }
        return true;
    }

private:
    static bool aabbCollision2D(const glm::vec3& boxPos,const glm::vec3& boxScale,
                                const glm::vec3& ballPos,float radius) {
        return (ballPos.x + radius > boxPos.x - boxScale.x*0.5f &&
                ballPos.x - radius < boxPos.x + boxScale.x*0.5f &&
                ballPos.y + radius > boxPos.y - boxScale.y*0.5f &&
                ballPos.y - radius < boxPos.y + boxScale.y*0.5f);
    }
};


//
// UISystem using ImGui: shows score/lives and Game Over modal with restart.
// Ensures popup opens only once when gameOver becomes true and closes on restart.
//
void UISystem(EntityManager& em, bool gameOver, bool &restartRequested, bool &popupOpened) {

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Game Info");
    for (auto &kv : em.scores) {
        ImGui::Text("Score: %d", kv.second.value);
    }
    for (auto &kv : em.lives) {
        ImGui::Text("Lives: %d", kv.second.value);
    }
    ImGui::End();

    if (gameOver) {
        if (!popupOpened) {
            ImGui::OpenPopup("Game Over!");
            popupOpened = true;
        }
    }

    if (ImGui::BeginPopupModal("Game Over!", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You lost all your lives!\n");
        ImGui::Separator();
        if (ImGui::Button("Restart")) {
            restartRequested = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Quit")) {
            // user can close window via ESC as well
            // we'll set restartRequested = false; and rely on main loop to exit if chosen
            restartRequested = false;
            // close popup but main loop will handle quit when requested externally.
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Render ImGui onto current framebuffer
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

//
// RenderSystem - draws bricks via instancing, draws paddle and ball normally
//
void RenderSystem(EntityManager &em, const glm::mat4& view, const glm::mat4& proj) {
    // update camera UBO
    updateCameraUBO(view, proj);

    // prepare instancing data for bricks
    buildInstancingBuffersForBricks(em);

    glUseProgram(glr.program);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // <--- PŘIDAT: Barva pozadí
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // <--- PŘIDAT: Vymazání bufferů
    // Draw bricks with instancing
    size_t instanceCount = em.brickOrder.size();
    if (instanceCount > 0) {
        glBindVertexArray(glr.instancedVAO);
        glDrawElementsInstanced(GL_TRIANGLES, glr.cubeMesh.indexCount, GL_UNSIGNED_INT, 0, (GLsizei)instanceCount);
        glBindVertexArray(0);
    }

    // Draw paddle and ball(s) individually (no instancing)
    // Paddle(s) use cube mesh
    for (auto &pp : em.paddles) {
        EntityID pid = pp.first;
        auto &t = em.transforms[pid];
        auto &r = em.renders[pid];
        glm::mat4 model(1.0f);
        model = glm::translate(model, t.pos);
        model = glm::rotate(model, t.angle, t.rotationAxis);
        model = glm::scale(model, t.scale);

        // set model & color via uniforms (shader expects instance attribs, but we can set as a uniform fallback)
        // We'll use a small fallback approach: create a temporary single-instance buffer and draw single element with instancing=1.
        // Simpler approach: use the shader but upload model as instance buffer and call glDrawElementsInstanced with count=1.
        // For clarity, we'll do that.
        // Upload single model
        glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4), glm::value_ptr(model), GL_DYNAMIC_DRAW);
        // upload single color
        glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4), glm::value_ptr(r.color), GL_DYNAMIC_DRAW);

        // draw single instance using instancedVAO (it uses cube mesh VBO/EBO)
        glBindVertexArray(glr.instancedVAO);
        glDrawElementsInstanced(GL_TRIANGLES, glr.cubeMesh.indexCount, GL_UNSIGNED_INT, 0, 1);
        glBindVertexArray(0);
    }

    // Ball(s) - draw sphere(s)
    // Use a simple path: reuse same shader but fill instance buffers with model & color for one instance
    for (auto &bb : em.balls) {
        EntityID bid = bb.first;
        auto &t = em.transforms[bid];
        auto &r = em.renders[bid];
        glm::mat4 model(1.0f);
        model = glm::translate(model, t.pos);
        model = glm::scale(model, t.scale);

        // upload model & color to instance buffers
        glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4), glm::value_ptr(model), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4), glm::value_ptr(r.color), GL_DYNAMIC_DRAW);

        // draw using sphere VAO but we must bind instance attributes to sphere VAO - easiest: bind instancedVAO's instance attributes to sphere VAO
        // We'll bind sphere VAO then rebind instance buffers to match locations 1..5
        glBindVertexArray(glr.sphereMesh.vao);

        // rebind instance matrix to locations 1..4 for sphere VAO
        glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
        GLsizei vec4Size = sizeof(glm::vec4);
        for (unsigned int i = 0; i < 4; ++i) {
            GLuint loc = 1 + i;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i*vec4Size));
            glVertexAttribDivisor(loc, 1);
        }
        // color
        glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
        glVertexAttribDivisor(5, 1);

        glDrawElementsInstanced(GL_TRIANGLES, glr.sphereMesh.indexCount, GL_UNSIGNED_INT, 0, 1);

        // cleanup: disable instance attributes on sphere vao to avoid interfering with later binds (optional)
        for (unsigned int i = 0; i < 4; ++i) {
            GLuint loc = 1 + i;
            glDisableVertexAttribArray(loc);
        }
        glDisableVertexAttribArray(5);
        glBindVertexArray(0);
    }
    // std::cout << "Entities count: " << em.entities.size() << "\n";
    // for (auto id : em.entities) {
    //     auto &r = em.renders[id];
    //     std::cout << "Entity " << id << " VAO=" << r.vao << " indexCount=" << r.indexCount << "\n";
    // }
    // restore
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

//
// -------------------- Game creation helper --------------------
//
static void setupGame(EntityManager &em, Mesh &cubeMesh, Mesh &ballMesh, EntityID &outPaddle, EntityID &outBall) {
    em.reset();

    // Paddle
    outPaddle = em.createEntity();
    em.transforms[outPaddle].pos = Constants::PADDLE_START_POS;
    em.transforms[outPaddle].scale = Constants::PADDLE_SCALE;
    em.renders[outPaddle].vao = cubeMesh.vao;
    em.renders[outPaddle].indexCount = cubeMesh.indexCount;
    em.renders[outPaddle].color = glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);
    em.paddles[outPaddle] = PaddleComponent{};

    // Bricks - create as normal ECS entities but also push id into brickOrder to maintain stable instance ordering
    for (int r=0; r<Constants::BRICK_ROWS; ++r) {
        for (int c=0; c<Constants::BRICK_COLS; ++c) {
            EntityID b = em.createEntity();
            float px = Constants::BRICK_OFFSET_X + c * Constants::BRICK_SPACING_X;
            float py = Constants::BRICK_OFFSET_Y + r * Constants::BRICK_SPACING_Y;
            em.transforms[b].pos = glm::vec3(px, py, 0.0f);
            em.transforms[b].scale = Constants::BRICK_SCALE;
            em.renders[b].vao = cubeMesh.vao;
            em.renders[b].indexCount = cubeMesh.indexCount;
            em.renders[b].color = glm::vec4(float(rand())/RAND_MAX, float(rand())/RAND_MAX, float(rand())/RAND_MAX, 1.0f);
            em.bricks[b] = BrickComponent{};
            em.brickOrder.push_back(b);
        }
    }

    // Ball
    outBall = em.createEntity();
    em.transforms[outBall].pos = Constants::BALL_START_POS;
    em.transforms[outBall].scale = glm::vec3(Constants::BALL_RADIUS);
    em.renders[outBall].vao = ballMesh.vao;
    em.renders[outBall].indexCount = ballMesh.indexCount;
    em.renders[outBall].color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
    em.balls[outBall] = BallComponent{Constants::BALL_START_VEL, Constants::BALL_RADIUS};

    // Single player score/lives entity (could be multiple players easily)
    EntityID player = em.createEntity();
    em.scores[player] = ScoreComponent{0};
    em.lives[player] = LivesComponent{Constants::INITIAL_LIVES};
}

//
// -------------------- main() --------------------
//
int main() {
    std::srand((unsigned int)std::time(nullptr));

    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n"; return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(Constants::SCR_WIDTH, Constants::SCR_HEIGHT, "ECS Arkanoid 3D", NULL, NULL);
    if (!window) { std::cerr << "Create window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD load failed\n"; glfwTerminate(); return -1;
    }
    glEnable(GL_DEPTH_TEST);

    initGLResources();

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    EntityManager em;
    EntityID paddle=0, ball=0;
    setupGame(em, glr.cubeMesh, glr.sphereMesh, paddle, ball);

    PhysicsSystem physics(em);

    float last = (float)glfwGetTime();
    bool running = true;
    bool restartRequested = false;
    bool popupOpened = false;
    glm::mat4 view = glm::lookAt(Constants::CAMERA_POS, glm::vec3(0.0f, 0.0f, 0.0f), Constants::CAMERA_UP);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), float(Constants::SCR_WIDTH) / float(Constants::SCR_HEIGHT), 0.1f, 100.0f);

    // --- MAIN LOOP ---
    while (!glfwWindowShouldClose(window)) {

        float now = (float)glfwGetTime();
        float dt = glm::clamp(now - last, 0.0f, 0.033f);
        last = now;

        glfwPollEvents();

        InputSystem(em, window, dt);

        if (running) {
            running = physics.update(dt);
            if (!running) popupOpened = false;
        }

        RenderSystem(em, view, proj);
        UISystem(em, !running, restartRequested, popupOpened);

        if (restartRequested) {
            setupGame(em, glr.cubeMesh, glr.sphereMesh, paddle, ball);
            physics.reset( em.transforms[paddle].pos,
                          em.transforms[ball].pos,
                          em.balls[ball].velocity );
            running = true;
            restartRequested = false;
            popupOpened = false;
            std::cout << "Game Restarted!\n";
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    cleanupGLResources();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
