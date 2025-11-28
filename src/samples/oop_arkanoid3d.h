// main.cpp
//  OOP Arkanoid 3D (single-file)

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
float fpsTimer = 0.0f;
int frameCount = 0;
// -------------------- Helpers --------------------
static void checkShaderCompile(GLuint shader, const char* name) {
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(shader, 1024, NULL, buf);
        std::cerr << "[Shader compile error] " << name << ":" << buf << "";
    }
}
static void checkProgramLink(GLuint prog, const char* name) {
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, 1024, NULL, buf);
        std::cerr << "[Program link error] " << name << ":" << buf << "";
    }
}

// -------------------- Mesh helpers --------------------
struct Mesh {
    GLuint vao=0,vbo=0, ebo=0;
    GLsizei indexCount = 0;
};

Mesh makeCube() {
    Mesh m;
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
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
    return m;
}

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

// -------------------- Shaders --------------------
const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
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
    vec3 V = normalize(vec3(0.0, 15.0, 35.0) - vWorldPos);
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 base = vColor.rgb * 0.6 + vColor.rgb * 0.4 * diff;
    vec3 color = base + vec3(1.0) * 0.6 * spec;
    FragColor = vec4(color, vColor.a);
}
)glsl";

// -------------------- GL Resources --------------------
struct GLResources {
    GLuint program = 0;
    GLuint cameraUBO = 0;
    Mesh cubeMesh;
    Mesh sphereMesh;
    GLuint instanceVBO = 0; // storage for model matrices (maxInstances)
    GLuint colorVBO = 0;    // storage for per-instance colors
    GLuint instancedCubeVAO = 0; // cube VAO with instance attributes bound
    GLuint instancedSphereVAO = 0; // sphere VAO with instance attributes bound
    size_t maxInstances = 0;
};
static GLResources glr;

// -------------------- Scene / OOP types --------------------
struct Transform {
    glm::vec3 pos{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotationAxis{0.0f,1.0f,0.0f};
    float angle = 0.0f;
};
struct Renderable {
    glm::vec4 color{1.0f};
    GLuint meshVao = 0;
    GLsizei indexCount = 0;
};

struct Brick {
    Transform transform;
    Renderable render;
    bool alive = true;
};

struct Paddle {
    Transform transform;
    Renderable render;
};

struct Ball {
    Transform transform;
    Renderable render;
    glm::vec3 velocity;
    float radius;
};

// -------------------- Utility: Configure instanced VAO once --------------------
static void configureInstancedVAO_forMesh(Mesh &mesh, GLuint vaoOut, GLuint instanceVBO, GLuint colorVBO) {
    glBindVertexArray(vaoOut);
    // base mesh VBO/ebo
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    // pos attrib already at location 0 in mesh VAO but this new VAO must re-specify it
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    // instance matrix (locations 1..4)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    GLsizei vec4Size = sizeof(glm::vec4);
    for (unsigned int i = 0; i < 4; ++i) {
        GLuint loc = 1 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
        glVertexAttribDivisor(loc, 1);
    }
    // color at location 5
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

// -------------------- Game class --------------------
class Game {
public:
    GLFWwindow* window = nullptr;
    std::vector<Brick> bricks; // contiguous storage
    Paddle paddle;
    Ball ball;
    int score = 0;
    int lives = Constants::INITIAL_LIVES;
    bool running = true;
    bool restartRequested = false;
    bool popupOpened = false;

    // preallocated instance data reused each frame
    std::vector<glm::mat4> instanceModels;
    std::vector<glm::vec4> instanceColors;

    glm::mat4 view;
    glm::mat4 proj;

    Game() {}

    bool initWindow() {
        if (!glfwInit()) {
            std::cerr << "GLFW init failed"; return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(Constants::SCR_WIDTH, Constants::SCR_HEIGHT, "OOP Arkanoid 3D (Optimized)", NULL, NULL);
        if (!window) { std::cerr << "Create window failed"; glfwTerminate(); return false; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD load failed"; glfwTerminate(); return false;
        }
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void initGLResources() {
        // compile shader program
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

        // camera UBO
        glGenBuffers(1, &glr.cameraUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, glr.cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::CAMERA_UBO_BINDING, glr.cameraUBO);
        GLuint blockIndex = glGetUniformBlockIndex(glr.program, "Camera");
        if (blockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(glr.program, blockIndex, Constants::CAMERA_UBO_BINDING);
        }

        // meshes
        glr.cubeMesh = makeCube();
        glr.sphereMesh = makeSphere(16, 16, Constants::BALL_RADIUS);

        // instance buffers: allocate for maximum possible instances
        glGenBuffers(1, &glr.instanceVBO);
        glGenBuffers(1, &glr.colorVBO);
        glr.maxInstances = (size_t)Constants::BRICK_ROWS * (size_t)Constants::BRICK_COLS + 4; // margin

        // allocate GPU storage once (orphanable later)
        glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
        glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // create VAOs which include instance attrib links (configured once)
        glGenVertexArrays(1, &glr.instancedCubeVAO);
        glGenVertexArrays(1, &glr.instancedSphereVAO);
        configureInstancedVAO_forMesh(glr.cubeMesh, glr.instancedCubeVAO, glr.instanceVBO, glr.colorVBO);
        configureInstancedVAO_forMesh(glr.sphereMesh, glr.instancedSphereVAO, glr.instanceVBO, glr.colorVBO);

        // preallocate CPU-side instance buffers
        instanceModels.reserve(glr.maxInstances);
        instanceColors.reserve(glr.maxInstances);
    }

    void cleanupGLResources() {
        if (glr.instanceVBO) glDeleteBuffers(1, &glr.instanceVBO);
        if (glr.colorVBO)    glDeleteBuffers(1, &glr.colorVBO);
        if (glr.cameraUBO)   glDeleteBuffers(1, &glr.cameraUBO);
        if (glr.instancedCubeVAO) glDeleteVertexArrays(1, &glr.instancedCubeVAO);
        if (glr.instancedSphereVAO) glDeleteVertexArrays(1, &glr.instancedSphereVAO);

        if (glr.cubeMesh.vbo) glDeleteBuffers(1, &glr.cubeMesh.vbo);
        if (glr.cubeMesh.ebo) glDeleteBuffers(1, &glr.cubeMesh.ebo);
        if (glr.cubeMesh.vao) glDeleteVertexArrays(1, &glr.cubeMesh.vao);

        if (glr.sphereMesh.vbo) glDeleteBuffers(1, &glr.sphereMesh.vbo);
        if (glr.sphereMesh.ebo) glDeleteBuffers(1, &glr.sphereMesh.ebo);
        if (glr.sphereMesh.vao) glDeleteVertexArrays(1, &glr.sphereMesh.vao);

        if (glr.program) glDeleteProgram(glr.program);
    }

    void updateCameraUBO(const glm::mat4& viewM, const glm::mat4& projM) {
        glBindBuffer(GL_UNIFORM_BUFFER, glr.cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(viewM));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projM));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    // -------------------- Setup game objects --------------------
    void setupGameObjects() {
        // bricks as value-type contiguous vector
        bricks.clear();
        bricks.reserve(Constants::BRICK_ROWS * Constants::BRICK_COLS);
        for (int r=0; r<Constants::BRICK_ROWS; ++r) {
            for (int c=0; c<Constants::BRICK_COLS; ++c) {
                Brick b;
                float px = Constants::BRICK_OFFSET_X + c * Constants::BRICK_SPACING_X;
                float py = Constants::BRICK_OFFSET_Y + r * Constants::BRICK_SPACING_Y;
                b.transform.pos = glm::vec3(px, py, 0.0f);
                b.transform.scale = Constants::BRICK_SCALE;
                b.render.meshVao = glr.cubeMesh.vao;
                b.render.indexCount = glr.cubeMesh.indexCount;
                b.render.color = glm::vec4(float(rand())/RAND_MAX, float(rand())/RAND_MAX, float(rand())/RAND_MAX, 1.0f);
                b.alive = true;
                bricks.push_back(std::move(b));
            }
        }

        // paddle
        paddle.transform.pos = Constants::PADDLE_START_POS;
        paddle.transform.scale = Constants::PADDLE_SCALE;
        paddle.render.meshVao = glr.cubeMesh.vao;
        paddle.render.indexCount = glr.cubeMesh.indexCount;
        paddle.render.color = glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);

        // ball
        ball.transform.pos = Constants::BALL_START_POS;
        ball.transform.scale = glm::vec3(Constants::BALL_RADIUS);
        ball.render.meshVao = glr.sphereMesh.vao;
        ball.render.indexCount = glr.sphereMesh.indexCount;
        ball.render.color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
        ball.velocity = Constants::BALL_START_VEL;
        ball.radius = Constants::BALL_RADIUS;

        score = 0;
        lives = Constants::INITIAL_LIVES;
        running = true;
        restartRequested = false;
        popupOpened = false;

        // ensure CPU-side instance arrays sized
        instanceModels.clear();
        instanceColors.clear();
        instanceModels.reserve(glr.maxInstances);
        instanceColors.reserve(glr.maxInstances);
    }

    // -------------------- Input --------------------
    void handleInput(float dt) {
        double mx,my;
        glfwGetCursorPos(window, &mx, &my);
        int w,h; glfwGetWindowSize(window, &w, &h);
        float worldX = static_cast<float>(mx) / float(w) * (Constants::WORLD_MAX_X - Constants::WORLD_MIN_X) + Constants::WORLD_MIN_X;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        // Paddle movement
        // keyboard
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) paddle.transform.pos.x -= Constants::PADDLE_SPEED * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) paddle.transform.pos.x += Constants::PADDLE_SPEED * dt;
        // mouse smooth follow
        float smooth = 10.0f;
        paddle.transform.pos.x += (worldX - paddle.transform.pos.x) * smooth * dt;
        // clamp
        float halfW = paddle.transform.scale.x * 0.5f;
        if (paddle.transform.pos.x - halfW < Constants::WORLD_MIN_X) paddle.transform.pos.x = Constants::WORLD_MIN_X + halfW;
        if (paddle.transform.pos.x + halfW > Constants::WORLD_MAX_X) paddle.transform.pos.x = Constants::WORLD_MAX_X - halfW;
    }

    // -------------------- Collision helper --------------------
    static bool aabbCollision2D(const glm::vec3& boxPos,const glm::vec3& boxScale,
                                const glm::vec3& ballPos,float radius) {
        return (ballPos.x + radius > boxPos.x - boxScale.x*0.5f &&
                ballPos.x - radius < boxPos.x + boxScale.x*0.5f &&
                ballPos.y + radius > boxPos.y - boxScale.y*0.5f &&
                ballPos.y - radius < boxPos.y + boxScale.y*0.5f);
    }

    // -------------------- Physics (single-threaded) --------------------
    void physicsUpdate(float dt) {
        // update ball
        ball.transform.pos += ball.velocity * dt;
        ball.transform.pos.z = 0.0f;

        // walls
        if (ball.transform.pos.x - ball.radius < Constants::WORLD_MIN_X) {
            ball.transform.pos.x = Constants::WORLD_MIN_X + ball.radius;
            ball.velocity.x *= -1.0f;
        }
        if (ball.transform.pos.x + ball.radius > Constants::WORLD_MAX_X) {
            ball.transform.pos.x = Constants::WORLD_MAX_X - ball.radius;
            ball.velocity.x *= -1.0f;
        }
        if (ball.transform.pos.y + ball.radius > Constants::WORLD_MAX_Y) {
            ball.transform.pos.y = Constants::WORLD_MAX_Y - ball.radius;
            ball.velocity.y *= -1.0f;
        }

        // paddle collision
        if (aabbCollision2D(paddle.transform.pos, paddle.transform.scale, ball.transform.pos, ball.radius)) {
            float rel = (ball.transform.pos.x - paddle.transform.pos.x) / (paddle.transform.scale.x * 0.5f);
            rel = glm::clamp(rel, -1.0f, 1.0f);
            float speed = glm::length(glm::vec2(ball.velocity.x, ball.velocity.y));
            float angle = rel * glm::radians(45.0f);
            ball.velocity.x = speed * sin(angle);
            ball.velocity.y = speed * cos(angle);
            ball.transform.pos.y = paddle.transform.pos.y + paddle.transform.scale.y * 0.5f + ball.radius;
        }

        // brick collisions: iterate contiguous bricks
        // Collect hits (indices) in a small stack style vector without allocations
        static std::vector<int> hits; // static to reuse capacity
        hits.clear();
        for (int i = 0; i < (int)bricks.size(); ++i) {
            Brick &b = bricks[i];
            if (!b.alive) continue;
            if (aabbCollision2D(b.transform.pos, b.transform.scale, ball.transform.pos, ball.radius)) {
                glm::vec2 dir = glm::vec2(ball.transform.pos.x - b.transform.pos.x, ball.transform.pos.y - b.transform.pos.y);
                if (std::abs(dir.x) > std::abs(dir.y)) ball.velocity.x *= -1.0f;
                else ball.velocity.y *= -1.0f;
                hits.push_back(i);
            }
        }
        if (!hits.empty()) {
            for (int idx : hits) {
                if (bricks[idx].alive) {
                    bricks[idx].alive = false;
                    score += Constants::SCORE_PER_BRICK;
                }
            }
        }

        // fell below world
        if (ball.transform.pos.y - ball.radius < Constants::WORLD_MIN_Y) {
            lives--;
            if (lives <= 0) {
                running = false;
                popupOpened = false;
            } else {
                // reset positions
                paddle.transform.pos = Constants::PADDLE_START_POS;
                ball.transform.pos = Constants::BALL_START_POS;
                ball.velocity = Constants::BALL_START_VEL;
            }
        }
    }

    // -------------------- Build instancing buffers (reuse CPU arrays)
    void buildInstancingBuffersForBricks() {
        instanceModels.clear();
        instanceColors.clear();
        for (const Brick &b : bricks) {
            if (!b.alive) continue;
            glm::mat4 model(1.0f);
            model = glm::translate(model, b.transform.pos);
            model = glm::rotate(model, b.transform.angle, b.transform.rotationAxis);
            model = glm::scale(model, b.transform.scale);
            instanceModels.push_back(model);
            instanceColors.push_back(b.render.color);
        }

        size_t aliveCount = instanceModels.size();
        if (aliveCount == 0) return;

        // Orphan + upload used prefix
        glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
        // orphan
        glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
        // upload used portion
        glBufferSubData(GL_ARRAY_BUFFER, 0, aliveCount * sizeof(glm::mat4), instanceModels.data());

        glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
        glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, aliveCount * sizeof(glm::vec4), instanceColors.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // -------------------- Rendering --------------------
    void renderFrame() {
        // update camera
        updateCameraUBO(view, proj);

        buildInstancingBuffersForBricks();

        glUseProgram(glr.program);
        glClearColor(0.2f,0.2f,0.2f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw bricks
        size_t instanceCount = instanceModels.size();
        if (instanceCount > 0) {
            glBindVertexArray(glr.instancedCubeVAO);
            glDrawElementsInstanced(GL_TRIANGLES, glr.cubeMesh.indexCount, GL_UNSIGNED_INT, 0, (GLsizei)instanceCount);
            glBindVertexArray(0);
        }

        // Draw paddle (single instance) using instanced VAO
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, paddle.transform.pos);
            model = glm::rotate(model, paddle.transform.angle, paddle.transform.rotationAxis);
            model = glm::scale(model, paddle.transform.scale);

            // update first slot of instance buffer (or use small temporary buffer) - we'll orphan and upload 1 matrix + color
            glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(model));
            glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
            glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(paddle.render.color));
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindVertexArray(glr.instancedCubeVAO);
            glDrawElementsInstanced(GL_TRIANGLES, glr.cubeMesh.indexCount, GL_UNSIGNED_INT, 0, 1);
            glBindVertexArray(0);
        }

        // Draw ball (sphere)
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, ball.transform.pos);
            model = glm::scale(model, ball.transform.scale);

            glBindBuffer(GL_ARRAY_BUFFER, glr.instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(model));
            glBindBuffer(GL_ARRAY_BUFFER, glr.colorVBO);
            glBufferData(GL_ARRAY_BUFFER, glr.maxInstances * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(ball.render.color));
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindVertexArray(glr.instancedSphereVAO);
            glDrawElementsInstanced(GL_TRIANGLES, glr.sphereMesh.indexCount, GL_UNSIGNED_INT, 0, 1);
            glBindVertexArray(0);
        }

        glUseProgram(0);
    }

    // -------------------- UI --------------------
    void renderUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Game Info");
        ImGui::Text("Score: %d", score);
        ImGui::Text("Lives: %d", lives);
        ImGui::End();

        if (!running) {
            if (!popupOpened) {
                ImGui::OpenPopup("Game Over!");
                popupOpened = true;
            }
        }

        if (ImGui::BeginPopupModal("Game Over!", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You lost all your lives!");
            ImGui::Separator();
            if (ImGui::Button("Restart")) {
                restartRequested = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Button("Quit")) {
                restartRequested = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // -------------------- Main loop --------------------
    void run() {
        if (!initWindow()) return;

        initGLResources();
        glfwSwapInterval(0);
        // ImGui init
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        setupGameObjects();

        view = glm::lookAt(Constants::CAMERA_POS, glm::vec3(0.0f,0.0f,0.0f), Constants::CAMERA_UP);
        proj = glm::perspective(glm::radians(45.0f), float(Constants::SCR_WIDTH) / float(Constants::SCR_HEIGHT), 0.1f, 100.0f);

        float last = (float)glfwGetTime();

        while (!glfwWindowShouldClose(window)) {
            float now = (float)glfwGetTime();
            float dt = glm::clamp(now - last, 0.0f, 0.033f);
            last = now;

            glfwPollEvents();

            handleInput(dt);

            if (running) physicsUpdate(dt);

            renderFrame();
            renderUI();

            if (restartRequested) {
                setupGameObjects();
                restartRequested = false;
                running = true;
                popupOpened = false;
            }

            glfwSwapBuffers(window);

            fpsTimer += dt;
            frameCount++;

            if (fpsTimer >= 1.0f) { // každou sekundu
                float avgFrameTime = 1000.0f * fpsTimer / frameCount; // ms
                float fps = frameCount / fpsTimer;
                std::cout << "[Profiler] Avg frame time: " << avgFrameTime
                          << " ms, FPS: " << fps << std::endl;
                fpsTimer = 0.0f;
                frameCount = 0;
            }
        }

        // shutdown ImGui and GL
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        cleanupGLResources();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

// -------------------- main --------------------
int main() {
    std::srand((unsigned int)std::time(nullptr));
    Game game;
    game.run();
    return 0;
}
