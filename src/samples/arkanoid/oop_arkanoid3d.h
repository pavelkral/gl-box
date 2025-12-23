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

namespace Config {


namespace Camera{
constexpr unsigned int SCREEN_WIDTH  = 1280;
constexpr unsigned int SCREEN_HEIGHT = 720;
inline constexpr glm::vec3 CAMERA_POS   = {0.0f, 8.0f, 95.0f};
inline constexpr glm::vec3 CAMERA_FRONT = {0.0f, -0.15f, -1.0f};
inline constexpr glm::vec3 CAMERA_UP    = {0.0f, 1.0f, 0.0f};
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
constexpr float SPEED = 50.0f;
}

namespace Ball {
inline constexpr glm::vec3 START_POS = {0.0f, -25.0f, 0.0f};
inline constexpr glm::vec3 START_VEL = {10.0f, 16.0f, 0.0f};
constexpr float RADIUS = 1.0f;
constexpr float SPEEDUP_FACTOR = 1.15f; // každá kolize zrychlí o 5%
constexpr float MAX_SPEED = 40.0f;
}
namespace Stats {
constexpr int INITIAL_LIVES = 3;
constexpr int SCORE_PER_BRICK = 10;
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
            // std::cout << "FPS: " << fps << " | " << std::endl;
        }
    }

    void drawUI(){
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
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

// -------------------- OpenGL RAII Wrappers --------------------
// GPU (destruktory)

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

    // Zakaz kopirovani (Resource ownership), povoleni move
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept : ID(other.ID) { other.ID = 0; }

    void use() const { glUseProgram(ID); }
    void setInt(const std::string& name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }

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
        if (!success) { glGetShaderInfoLog(shader, 1024, NULL, infoLog); std::cerr << "SHADER_ERROR::" << type << "\n" << infoLog << std::endl; }
    }
    void checkLinkErrors(GLuint program) {
        GLint success; char infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) { glGetProgramInfoLog(program, 1024, NULL, infoLog); std::cerr << "PROGRAM_LINK_ERROR\n" << infoLog << std::endl; }
    }
};

class Buffer {
public:
    GLuint ID = 0;
    GLenum type;

    Buffer(GLenum type) : type(type) { glGenBuffers(1, &ID); }
    ~Buffer() { if(ID) glDeleteBuffers(1, &ID); }

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept : ID(other.ID), type(other.type) { other.ID = 0; }

    void bind() const { glBindBuffer(type, ID); }
    void unbind() const { glBindBuffer(type, 0); }

    template<typename T>
    void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
        bind();
        glBufferData(type, data.size() * sizeof(T), data.data(), usage);
    }

    void allocate(size_t size, GLenum usage = GL_DYNAMIC_DRAW) {
        bind();
        glBufferData(type, size, nullptr, usage);
    }

    template<typename T>
    void setSubData(const std::vector<T>& data, size_t offset = 0) {
        bind();
        glBufferSubData(type, offset, data.size() * sizeof(T), data.data());
    }

    // Pro jednu instanci
    template<typename T>
    void setSubDataSingle(const T& data, size_t offset = 0) {
        bind();
        glBufferSubData(type, offset, sizeof(T), &data);
    }
};

class VertexArray {
public:
    GLuint ID = 0;
    VertexArray() { glGenVertexArrays(1, &ID); }
    ~VertexArray() { if(ID) glDeleteVertexArrays(1, &ID); }

    VertexArray(const VertexArray&) = delete;
    VertexArray(VertexArray&& other) noexcept : ID(other.ID) { other.ID = 0; }

    void bind() const { glBindVertexArray(ID); }
    void unbind() const { glBindVertexArray(0); }
};

// -------------------- Mesh --------------------
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

        // Layout 0: Pos
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

// Helpery pro generovani meshu
namespace MeshFactory {
std::unique_ptr<Mesh> createCube() {
    std::vector<float> vertices = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    std::vector<unsigned int> indices = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 0,1,5, 5,4,0, 2,3,7, 7,6,2, 0,3,7, 7,4,0, 1,2,6, 6,5,1
    };
    return std::make_unique<Mesh>(vertices, indices);
}

std::unique_ptr<Mesh> createSphere(float radius = 1.0f, int latSeg = 16, int longSeg = 16) {
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
            indices.push_back((unsigned int)a);
            indices.push_back((unsigned int)b);
            indices.push_back((unsigned int)(a+1));
            indices.push_back((unsigned int)b);
            indices.push_back((unsigned int)(b+1));
            indices.push_back((unsigned int)(a+1));
        }
    }
    return std::make_unique<Mesh>(vertices, indices);
}
}

// -------------------- Game Objects --------------------
struct Transform {
    glm::vec3 pos{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 getMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, pos);
        m = glm::scale(m, scale);
        return m;
    }
};

struct Brick {
    Transform transform;
    glm::vec4 color;
    glm::mat4 cachedMatrix; // Optimalizace: vypočítáme jen jednou
    bool alive = true;

    Brick(glm::vec3 p, glm::vec3 s, glm::vec4 c) : color(c) {
        transform.pos = p;
        transform.scale = s;
        cachedMatrix = transform.getMatrix();
    }
};

struct Paddle {
    Transform transform;
    glm::vec4 color{0.3f, 0.8f, 0.3f, 1.0f};
};

struct Ball {
    Transform transform;
    glm::vec3 velocity;
    float radius;
    glm::vec4 color{1.0f, 0.2f, 0.2f, 1.0f};
};

// -------------------- Shaders Code --------------------
// note: přidáno binding = 0 pro UBO
const char* VS_SRC = R"glsl(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aRow0; // Instance Matrix Row 0
layout(location = 2) in vec4 aRow1;
layout(location = 3) in vec4 aRow2;
layout(location = 4) in vec4 aRow3;
layout(location = 5) in vec4 aColor; // Instance Color

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
};

out vec3 vPos;
out vec3 vNormal;
out vec4 vColor;

void main() {
    mat4 model = mat4(aRow0, aRow1, aRow2, aRow3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vPos = worldPos.xyz;
    vNormal = normalize(mat3(model) * aPos); // Simplified normal
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
    vec3 lightPos = vec3(10.0, 20.0, 10.0);
    vec3 L = normalize(lightPos - vPos);
    vec3 V = normalize(vec3(0.0, 15.0, 35.0) - vPos); // camera pos baked
    // simple Blinn-Phong
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 base = vColor.rgb * 0.6 + vColor.rgb * 0.4 * diff;
    vec3 color = base + vec3(1.0) * 0.6 * spec; // add highlight
    FragColor = vec4(color, vColor.a);
}
)glsl";

// -------------------- Game Class --------------------
class Game {
    // Custom deleter pro GLFW window
    struct WindowDeleter { void operator()(GLFWwindow* ptr) { glfwDestroyWindow(ptr); } };
    std::unique_ptr<GLFWwindow, WindowDeleter> window;

    // Zdroje
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<Mesh> sphereMesh;

    // UBO a Instance Buffery
    std::unique_ptr<Buffer> uboCamera;
    std::unique_ptr<Buffer> vboInstance; // Matice (Model)
    std::unique_ptr<Buffer> vboColor;    // Barvy

    // Game Objects
    std::vector<Brick> bricks;
    Paddle paddle;
    Ball ball;
    glm::mat4 view;
    glm::mat4 proj;
    // State
    bool running = true;
    bool gameOver = false;
    bool gameWon = false;
    bool ballLaunched = false; // nový stav: jestli je míček vypuštěn
    int score = 0;
    int lives = Config::Stats::INITIAL_LIVES;
    float fpsTimer = 0.0f;
    int frameCount = 0;
    Stats stats;
    // Temporary vectors for instancing (avoid reallocations)
    std::vector<glm::mat4> renderMatrices;
    std::vector<glm::vec4> renderColors;

    // Pro faleš / spin: uložíme poslední x pozici paddle a horizontální rychlost
    float lastPaddleX = Config::Paddle::START_POS.x;
    float paddleVelocityX = 0.0f;

public:
    Game() = default;

    [[nodiscard]] bool init() {

        if (!glfwInit()) return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window.reset(glfwCreateWindow(Config::Camera::SCREEN_WIDTH, Config::Camera::SCREEN_HEIGHT, "Arkanoid 3D", nullptr, nullptr));
        if (!window) return false;

        glfwMakeContextCurrent(window.get());
        glfwSwapInterval(0); // VSync off for benchmarking

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
        // Init ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        initResources();
        resetGame();
        return true;
    }

    void run() {
        view = glm::lookAt(Config::Camera::CAMERA_POS,Config::Camera::CAMERA_POS + Config::Camera::CAMERA_FRONT,Config::Camera::CAMERA_UP);
        proj = glm::perspective(glm::radians(45.0f), (float)Config::Camera::SCREEN_WIDTH / Config::Camera::SCREEN_HEIGHT, 0.1f, 100.0f);

        float lastTime = (float)glfwGetTime();
        while (!glfwWindowShouldClose(window.get())) {
            float now = (float)glfwGetTime();
            float dt = std::min(now - lastTime, 0.05f); // Clamp dt
            lastTime = now;

            glfwPollEvents();
            processInput(dt);

            if (!gameOver) {
                updatePhysics(dt);
            }

            render();
            renderUI();

            glfwSwapBuffers(window.get());

            stats.update(dt);
        }
        cleanup();
    }

private:
    void initResources() {
        shader = std::make_unique<Shader>(VS_SRC, FS_SRC);
        cubeMesh = MeshFactory::createCube();
        sphereMesh = MeshFactory::createSphere(Config::Ball::RADIUS);

        // Camera UBO setup
        uboCamera = std::make_unique<Buffer>(GL_UNIFORM_BUFFER);
        uboCamera->allocate(2 * sizeof(glm::mat4));
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboCamera->ID);

        // Instance Buffers setup (Allocate Max size)
        size_t maxInstances = Config::Bricks::ROWS * Config::Bricks::COLS + 10;
        vboInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboInstance->allocate(maxInstances * sizeof(glm::mat4));

        vboColor = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
        vboColor->allocate(maxInstances * sizeof(glm::vec4));

        // Configure VAOs for Instancing
        auto setupAttribs = [&](VertexArray& vao) {
            vao.bind();
            // Bind instance VBOs to this VAO
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
        };

        setupAttribs(*cubeMesh->vao);
        setupAttribs(*sphereMesh->vao);

        renderMatrices.reserve(maxInstances);
        renderColors.reserve(maxInstances);
    }

    void resetGame() {
        score = 0;
        lives = Config::Stats::INITIAL_LIVES;
        gameOver = false;
        gameWon = false;
        ballLaunched = false;

        bricks.clear();

        int rows = Config::Bricks::ROWS; // počet řad
        int cols = Config::Bricks::COLS; // počet sloupců

        float totalWidth = Config::World::MAX_X - Config::World::MIN_X;
        float spacingX = 0.2f; // mezera mezi bricks

        // vypočítáme šířku každého bricku tak, aby vyplnil šířku pole
        float brickWidth = (totalWidth - (cols - 1) * spacingX) / cols;
        float brickHeight = Config::Bricks::SCALE.y; // zachováme výšku
        float startX = Config::World::MIN_X + brickWidth * 0.5f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                glm::vec3 pos = {
                    startX + c * (brickWidth + spacingX),
                    Config::Bricks::START_Y + r * (brickHeight + spacingX),
                    0.0f
                };
                glm::vec3 scale = {brickWidth, brickHeight, Config::Bricks::SCALE.z};
                bricks.emplace_back(pos, scale, Random::RandomColor());
            }
        }

        resetBallPaddle();
    }

    void resetBallPaddle() {
        paddle.transform.pos = Config::Paddle::START_POS;
        paddle.transform.scale = Config::Paddle::SCALE;

        ballLaunched = false;
        ball.transform.scale = glm::vec3(Config::Ball::RADIUS, Config::Ball::RADIUS, Config::Ball::RADIUS); // Scale vizual
        ball.radius = Config::Ball::RADIUS;
        ball.velocity = Config::Ball::START_VEL;

        // ensure paddle tracking variables reset
        lastPaddleX = paddle.transform.pos.x;
        paddleVelocityX = 0.0f;
    }

    void processInput(float dt) {
        if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window.get(), true);

        // Mouse control
        double xpos, ypos;
        glfwGetCursorPos(window.get(), &xpos, &ypos);
        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);

        // Map mouse X to World X
        float normX = (float)xpos / width; // 0..1
        float worldX = Config::World::MIN_X + normX * (Config::World::MAX_X - Config::World::MIN_X);

        // Smooth lerp with paddle velocity calculation for spin
        float prevPaddleX = paddle.transform.pos.x;
        paddle.transform.pos.x += (worldX - paddle.transform.pos.x) * 15.0f * dt;

        // Clamp
        float halfW = paddle.transform.scale.x * 0.5f;
        paddle.transform.pos.x = std::clamp(paddle.transform.pos.x, Config::World::MIN_X + halfW, Config::World::MAX_X - halfW);

        // compute paddle horizontal velocity (used for "faleš")
        // guard dt > 0
        if (dt > 0.0f) {
            paddleVelocityX = (paddle.transform.pos.x - prevPaddleX) / dt;
        } else {
            paddleVelocityX = 0.0f;
        }
        lastPaddleX = paddle.transform.pos.x;

        // Space to launch ball
        if (glfwGetKey(window.get(), GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!ballLaunched && !gameOver && !gameWon) {
                ballLaunched = true;
            }
        }
    }

    bool checkAABB(const glm::vec3& bPos, const glm::vec3& bScale, const glm::vec3& spherePos, float r) {
        // Simple AABB vs Point/Radius approximation
        float halfW = bScale.x * 0.5f;
        float halfH = bScale.y * 0.5f;
        return (spherePos.x + r > bPos.x - halfW && spherePos.x - r < bPos.x + halfW &&
                spherePos.y + r > bPos.y - halfH && spherePos.y - r < bPos.y + halfH);
    }

    glm::vec3 reflectVector(const glm::vec3& v, const glm::vec3& normal) {
        return v - 2.0f * glm::dot(v, normal) * normal;
    }

    void updatePhysics(float dt) {
        // pokud míček ještě nebyl vypuštěn, držíme ho nad středem paddle
        if (!ballLaunched) {
            ball.transform.pos.x = paddle.transform.pos.x;
            ball.transform.pos.y = paddle.transform.pos.y + paddle.transform.scale.y * 0.5f + ball.radius + 0.2f;
            return;
        }

        ball.transform.pos += ball.velocity * dt;

        // Walls
        if (ball.transform.pos.x <= Config::World::MIN_X) {
            ball.transform.pos.x = Config::World::MIN_X;
            ball.velocity.x *= -1.0f;
        } else if (ball.transform.pos.x >= Config::World::MAX_X) {
            ball.transform.pos.x = Config::World::MAX_X;
            ball.velocity.x *= -1.0f;
        }
        if (ball.transform.pos.y >= Config::World::MAX_Y) {
            ball.transform.pos.y = Config::World::MAX_Y;
            ball.velocity.y *= -1.0f;
        }

        // Paddle Collision
        if (checkAABB(paddle.transform.pos, paddle.transform.scale, ball.transform.pos, ball.radius)) {
            // Normála je nahoru pro paddle
            glm::vec3 normal(0.0f, 1.0f, 0.0f);

            // Use physical reflection
            ball.velocity = reflectVector(ball.velocity, normal);

            // Add "faleš" (spin) based on paddle horizontal velocity
            // menší koeficient, aby to nebylo extrémní
            ball.velocity.x += paddleVelocityX * 0.12f;

            // posuneme míček ven, aby nebyl uvnitř kolize
            ball.transform.pos.y = paddle.transform.pos.y + paddle.transform.scale.y * 0.5f + ball.radius + 0.1f;

            // zrychlení
            ball.velocity *= Config::Ball::SPEEDUP_FACTOR;
            if (glm::length(ball.velocity) > Config::Ball::MAX_SPEED)
                ball.velocity = glm::normalize(ball.velocity) * Config::Ball::MAX_SPEED;
        }

        // Brick Collision (Iterate through bricks)
        for (auto& brick : bricks) {
            if (!brick.alive) continue;
            if (checkAABB(brick.transform.pos, brick.transform.scale, ball.transform.pos, ball.radius)) {
                brick.alive = false;
                score += Config::Stats::SCORE_PER_BRICK;

                // determine approximate collision normal by comparing delta
                glm::vec3 delta = ball.transform.pos - brick.transform.pos;

                glm::vec3 normal;
                if (std::abs(delta.x) > std::abs(delta.y)) {
                    normal = glm::vec3(delta.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
                } else {
                    normal = glm::vec3(0.0f, delta.y > 0 ? 1.0f : -1.0f, 0.0f);
                }

                ball.velocity = reflectVector(ball.velocity, normal);

                // speedup
                ball.velocity *= Config::Ball::SPEEDUP_FACTOR;
                float len = glm::length(ball.velocity);
                if (len > Config::Ball::MAX_SPEED)
                    ball.velocity = glm::normalize(ball.velocity) * Config::Ball::MAX_SPEED;
                break;  //stop
            }
        }

        // Death
        if (ball.transform.pos.y < Config::World::MIN_Y) {
            lives--;
            if (lives <= 0) {
                gameOver = true;
                ballLaunched = false;
            } else {
                resetBallPaddle();
            }
        }

        // Victory
        bool allDestroyed = true;
        for (const auto& brick : bricks) {
            if (brick.alive) { allDestroyed = false; break; }
        }
        if (allDestroyed) {
            gameWon = true;
            gameOver = true;
            ballLaunched = false;
        }
    }

    void render() {

        uboCamera->bind();
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));
        uboCamera->unbind();

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();

        // --- Render Bricks (Instanced) ---
        renderMatrices.clear();
        renderColors.clear();

        // 1. Collect Active Bricks
        for (const auto& brick : bricks) {
            if (brick.alive) {
                renderMatrices.push_back(brick.cachedMatrix); // No recalculation!
                renderColors.push_back(brick.color);
            }
        }

        // 2. Draw Bricks
        if (!renderMatrices.empty()) {
            vboInstance->setSubData(renderMatrices);
            vboColor->setSubData(renderColors);
            cubeMesh->drawInstanced((GLsizei)renderMatrices.size());
        }

        // --- Render Paddle & Ball ---
        // Reuse vectors for dynamic objects
        renderMatrices.clear();
        renderColors.clear();

        renderMatrices.push_back(paddle.transform.getMatrix());
        renderColors.push_back(paddle.color);

        vboInstance->setSubDataSingle(renderMatrices[0]);
        vboColor->setSubDataSingle(renderColors[0]);
        cubeMesh->drawInstanced(1); // Draw Paddle

        // Draw Ball
        renderMatrices[0] = ball.transform.getMatrix();
        renderColors[0] = ball.color;

        vboInstance->setSubDataSingle(renderMatrices[0]);
        vboColor->setSubDataSingle(renderColors[0]);
        sphereMesh->drawInstanced(1); // Draw Ball
    }

    void renderUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Score: %d", score);
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Lives: %d", lives);
        ImGui::End();

        if (gameOver && !gameWon) {
            ImGui::OpenPopup("GameOver");
        }

        if (ImGui::BeginPopupModal("GameOver", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("GAME OVER");
            ImGui::Text("Final Score: %d", score);
            if (ImGui::Button("Restart", ImVec2(120, 0))) {
                resetGame();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit", ImVec2(120, 0))) {
                glfwSetWindowShouldClose(window.get(), true);
            }
            ImGui::EndPopup();
        }

        // Game Won popup
        if (gameWon) {
            ImGui::OpenPopup("GameWon");
        }

        if (ImGui::BeginPopupModal("GameWon", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("CONGRATULATIONS!");
            ImGui::Text("You destroyed all bricks!");
            ImGui::Text("Final Score: %d", score);
            if (ImGui::Button("Restart", ImVec2(120, 0))) {
                resetGame();
                gameWon = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit", ImVec2(120, 0))) {
                glfwSetWindowShouldClose(window.get(), true);
            }
            ImGui::EndPopup();
        }

        stats.drawUI();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        // GL resources are destroyed automatically by unique_ptr destructors!
    }
};

int main() {
    Game game;
    if (game.init()) {
        game.run();
    } else {
        std::cerr << "Failed to initialize game." << std::endl;
        return -1;
    }
    return 0;
}
