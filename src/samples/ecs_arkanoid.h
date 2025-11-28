// main.cpp - jednoduchý Arkanoid (ECS + OpenGL)
// Vyžaduje: GLAD, GLFW, GLM
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;


const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 model;
uniform mat4 projection;

void main()
{
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main()
{
    FragColor = uColor;
}
)glsl";

// --- ECS komponents & manager ---
using EntityID = unsigned int;
struct PositionComponent { float x=0,y=0; };
struct VelocityComponent { float x=0,y=0; };
struct RenderComponent { float w=1.0f, h=1.0f; }; // width, height
struct ColorComponent { float r=1,g=1,b=1,a=1; };
struct GameplayComponent { int type=0; /*0=none,1=paddle,2=ball,3=brick*/ int health=1; };

struct WorldMatrixComponent { glm::mat4 matrix{1.0f}; };

class EntityManager {
public:
    std::vector<EntityID> entities;
    std::vector<PositionComponent> positions;
    std::vector<VelocityComponent> velocities;
    std::vector<RenderComponent> renders;
    std::vector<ColorComponent> colors;
    std::vector<GameplayComponent> gameplay;
    std::vector<WorldMatrixComponent> matrices;
    std::vector<bool> alive;

private:
    EntityID nextID = 0;

public:
    EntityID createEntity() {
        EntityID id = nextID++;
        entities.push_back(id);
        positions.emplace_back();
        velocities.emplace_back();
        renders.emplace_back();
        colors.emplace_back();
        gameplay.emplace_back();
        matrices.emplace_back();
        alive.push_back(true);
        return id;
    }
    void destroyEntity(EntityID id) {
        if (id < alive.size()) alive[id] = false;
    }
    bool isAlive(EntityID id) {
        return id < alive.size() && alive[id];
    }
    size_t size() const { return entities.size(); }
};

// --- utility: AABB vs circle collision ---
bool aabb_circle_collision(float ax, float ay, float aw, float ah,
                           float cx, float cy, float cr,
                           glm::vec2 &outNormal)
{
    // Find closest point on AABB to circle center
    float closestX = std::max(ax - aw/2.0f, std::min(cx, ax + aw/2.0f));
    float closestY = std::max(ay - ah/2.0f, std::min(cy, ay + ah/2.0f));
    float dx = cx - closestX;
    float dy = cy - closestY;
    float dist2 = dx*dx + dy*dy;
    if (dist2 > cr*cr) return false;
    // Normal from collision: from box to circle center (approx)
    glm::vec2 normal = glm::vec2(cx - ax, cy - ay);
    if (glm::length(normal) < 1e-6f) normal = glm::vec2(0.0f, 1.0f);
    outNormal = glm::normalize(normal);
    return true;
}

// --- OpenGL helper ---
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

// --- Globals for game ---
EntityManager em;
GLuint shaderProgram;
GLuint quadVAO, quadVBO;
glm::mat4 projection;
int score = 0;
int lives = 3;
bool leftPressed=false, rightPressed=false;

// --- Input callbacks ---
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) {
        leftPressed = action != GLFW_RELEASE;
    }
    if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) {
        rightPressed = action != GLFW_RELEASE;
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        // If ball stuck to paddle, launch it (handled in BallSystem)
    }
}

// --- Init quad mesh (unit centered rectangle -1..1 scaled by model) ---
void initQuad() {
    float quadVertices[] = {
        // two triangles forming a unit rectangle centered at origin (width=1,height=1)
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.5f,  0.5f,
        0.5f,  0.5f,
        -0.5f,  0.5f,
        -0.5f, -0.5f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

// --- Create shader program ---
GLuint createShaderProgram() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs,1,&vertexShaderSource,nullptr);
    glCompileShader(vs);
    checkShaderCompileErrors(vs, "VERTEX");

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&fragmentShaderSource,nullptr);
    glCompileShader(fs);
    checkShaderCompileErrors(fs, "FRAGMENT");

    GLuint prog = glCreateProgram();
    glAttachShader(prog,vs);
    glAttachShader(prog,fs);
    glLinkProgram(prog);
    checkShaderCompileErrors(prog, "PROGRAM");

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// --- Systems ---

// 1) Input / Paddle movement system
void PaddleSystem(float deltaTime) {
    float paddleSpeed = 800.0f; // units/sec in screen coordinates
    for (size_t i=0;i<em.size();++i) {
        if (!em.isAlive(i)) continue;
        if (em.gameplay[i].type == 1) { // paddle
            float dir = 0.0f;
            if (leftPressed) dir -= 1.0f;
            if (rightPressed) dir += 1.0f;
            em.positions[i].x += dir * paddleSpeed * deltaTime;
            // clamp to screen (world coordinates: we'll use ortho with width same as window)
            float halfW = em.renders[i].w/2.0f;
            float minX = halfW;
            float maxX = (float)SCR_WIDTH - halfW;
            if (em.positions[i].x < minX) em.positions[i].x = minX;
            if (em.positions[i].x > maxX) em.positions[i].x = maxX;
        }
    }
}

// 2) Ball system: movement + collisions
void BallSystem(float deltaTime) {
    // Find ball entity
    int ballId = -1;
    int paddleId = -1;
    for (size_t i=0;i<em.size();++i) {
        if (!em.isAlive(i)) continue;
        if (em.gameplay[i].type == 2) ballId = (int)i;
        if (em.gameplay[i].type == 1) paddleId = (int)i;
    }
    if (ballId == -1) return;

    PositionComponent &bp = em.positions[ballId];
    VelocityComponent &bv = em.velocities[ballId];
    float radius = em.renders[ballId].w * 0.5f; // uses w as diameter

    // move
    bp.x += bv.x * deltaTime;
    bp.y += bv.y * deltaTime;

    // Walls (left/right/top)
    if (bp.x - radius <= 0.0f) {
        bp.x = radius;
        bv.x = -bv.x;
    }
    if (bp.x + radius >= (float)SCR_WIDTH) {
        bp.x = (float)SCR_WIDTH - radius;
        bv.x = -bv.x;
    }
    if (bp.y + radius >= (float)SCR_HEIGHT) {
        bp.y = (float)SCR_HEIGHT - radius;
        bv.y = -bv.y;
    }

    // Bottom: lose life
    if (bp.y - radius <= 0.0f) {
        lives--;
        std::cout << "Life lost! Remaining: " << lives << std::endl;
        // reset ball and paddle to initial positions (simple)
        // find paddle center X
        if (paddleId != -1) {
            em.positions[paddleId].x = (float)SCR_WIDTH * 0.5f;
            em.positions[paddleId].y = 50.0f;
        }
        bp.x = (float)SCR_WIDTH * 0.5f;
        bp.y = 75.0f;
        bv.x = 250.0f * ( (rand()%2)?1:-1 );
        bv.y = 350.0f;
        if (lives <= 0) {
            std::cout << "GAME OVER - Score: " << score << std::endl;
            // reset game
            lives = 3;
            score = 0;
            // respawn bricks
            // mark all bricks alive again (we will set depending on creation)
            for (size_t i=0;i<em.size();++i) {
                if (em.gameplay[i].type == 3) em.alive[i]=true;
            }
        }
        return;
    }

    // Collision with paddle
    if (paddleId != -1 && em.isAlive(paddleId)) {
        glm::vec2 normal;
        if (aabb_circle_collision(em.positions[paddleId].x, em.positions[paddleId].y, em.renders[paddleId].w, em.renders[paddleId].h,
                                  bp.x, bp.y, radius, normal)) {
            // reflect velocity across normal
            glm::vec2 v = glm::vec2(bv.x,bv.y);
            glm::vec2 n = glm::normalize(normal);
            glm::vec2 r = v - 2.0f * glm::dot(v,n) * n;
            bv.x = r.x;
            bv.y = r.y;
            // add spin based on where it hit paddle
            float rel = (bp.x - em.positions[paddleId].x) / (em.renders[paddleId].w*0.5f); // -1..1
            bv.x += rel * 150.0f;
            // ensure up direction
            if (bv.y < 100.0f) bv.y = 100.0f;
            // push ball out
            bp.y = em.positions[paddleId].y + em.renders[paddleId].h/2.0f + radius + 0.5f;
        }
    }

    // Collision with bricks
    for (size_t i=0;i<em.size();++i) {
        if (!em.isAlive(i)) continue;
        if (em.gameplay[i].type != 3) continue;
        glm::vec2 normal;
        if (aabb_circle_collision(em.positions[i].x, em.positions[i].y, em.renders[i].w, em.renders[i].h,
                                  bp.x, bp.y, radius, normal)) {
            // destroy brick
            em.destroyEntity((EntityID)i);
            score += 100;
            // reflect ball
            glm::vec2 v = glm::vec2(bv.x,bv.y);
            glm::vec2 n = glm::normalize(normal);
            glm::vec2 r = v - 2.0f * glm::dot(v,n) * n;
            bv.x = r.x;
            bv.y = r.y;
            // Slight speed-up
           // bv *= 1.02f;
            bv.x *= 1.02f;
            bv.y *= 1.02f;
            break; // only one brick per frame
        }
    }

}

// 3) Render system
void RenderSystem() {
    glUseProgram(shaderProgram);
    glBindVertexArray(quadVAO);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    for (size_t i=0;i<em.size();++i) {
        if (!em.isAlive(i)) continue;
        // world matrix: translate to position and scale by render w/h
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(em.positions[i].x, em.positions[i].y, 0.0f));
        model = glm::scale(model, glm::vec3(em.renders[i].w, em.renders[i].h, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4f(colorLoc, em.colors[i].r, em.colors[i].g, em.colors[i].b, em.colors[i].a);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
}

// --- Helper: create bricks grid ---
void createBricksGrid(int cols, int rows, float startX, float startY, float brickW, float brickH, float padding) {
    for (int r=0;r<rows;++r) {
        for (int c=0;c<cols;++c) {
            EntityID id = em.createEntity();
            em.gameplay[id].type = 3;
            em.positions[id].x = startX + c * (brickW + padding);
            em.positions[id].y = startY - r * (brickH + padding);
            em.renders[id].w = brickW;
            em.renders[id].h = brickH;
            float hue = (float)r / (float)rows;
            em.colors[id].r = 0.3f + 0.7f * ((float)rand()/RAND_MAX);
            em.colors[id].g = 0.3f + 0.7f * ((float)rand()/RAND_MAX);
            em.colors[id].b = 0.3f + 0.7f * ((float)rand()/RAND_MAX);
        }
    }
}

// --- Setup initial game entities (paddle + ball + bricks) ---
void setupGame() {

    // paddle
    EntityID paddle = em.createEntity();
    em.gameplay[paddle].type = 1;
    em.renders[paddle].w = 200.0f;
    em.renders[paddle].h = 20.0f;
    em.positions[paddle].x = (float)SCR_WIDTH * 0.5f;
    em.positions[paddle].y = 50.0f;
    em.colors[paddle].r = 0.8f; em.colors[paddle].g = 0.8f; em.colors[paddle].b = 0.2f;

    // ball
    EntityID ball = em.createEntity();
    em.gameplay[ball].type = 2;
    em.renders[ball].w = 16.0f; em.renders[ball].h = 16.0f; // ball uses w as diameter
    em.positions[ball].x = (float)SCR_WIDTH * 0.5f;
    em.positions[ball].y = 75.0f;
    em.velocities[ball].x = 200.0f * ((rand()%2)?1:-1);
    em.velocities[ball].y = 300.0f;
    em.colors[ball].r = 1.0f; em.colors[ball].g = 0.5f; em.colors[ball].b = 0.2f;

    // bricks
    int cols = 10;
    int rows = 6;
    float brickW = 100.0f;
    float brickH = 30.0f;
    float pad = 8.0f;
    float totalW = cols * brickW + (cols-1) * pad;
    float startX = ((float)SCR_WIDTH - totalW) * 0.5f + brickW*0.5f;
    float startY = (float)SCR_HEIGHT - 120.0f;
    createBricksGrid(cols, rows, startX, startY, brickW, brickH, pad);
}

// --- Main ---
int main() {
    srand((unsigned int)time(nullptr));

    // GLFW init
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Arkanoid - ECS OpenGL", NULL, NULL);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);
    // no cursor manipulation needed

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to init GLAD\n"; return -1; }
    glViewport(0,0,SCR_WIDTH,SCR_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // shaders + quad
    shaderProgram = createShaderProgram();
    initQuad();

    // orthographic projection matching pixel coordinates (origin at bottom-left)
    projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT, -1.0f, 1.0f);

    // init game
    setupGame();

    float lastTime = (float)glfwGetTime();

    // main loop
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        if (dt > 0.033f) dt = 0.033f; // clamp
        lastTime = currentTime;

        glfwPollEvents();

        // Systems
        PaddleSystem(dt);
        BallSystem(dt);

        // Render
        glClearColor(0.05f,0.05f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        RenderSystem();

        // HUD: print to console occasionally (simple)
        static float hudTimer=0.0f;
        hudTimer += dt;
        if (hudTimer > 1.0f) {
            hudTimer = 0.0f; std::cout << "Score: " << score << "  Lives: " << lives << std::endl;
        }

        glfwSwapBuffers(window);
    }

    // cleanup
    glDeleteVertexArrays(1,&quadVAO);
    glDeleteBuffers(1,&quadVBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
