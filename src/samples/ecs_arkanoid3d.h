// main.cpp
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

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------- Camera ----------------
glm::vec3 cameraPos(0.0f, 15.0f, 35.0f);
glm::vec3 cameraFront(0.0f, -0.3f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

// ---------------- ECS ----------------
using EntityID = unsigned int;

struct TransformComponent{
    glm::vec3 pos;
    glm::vec3 scale{1.0f};
    glm::vec3 rotationAxis{0,1,0};
    float angle=0.0f;
};

struct RenderComponent{
    GLuint vao=0;
    GLsizei indexCount=0;
    glm::vec4 color{1,1,1,1};
};

struct BallComponent{
    glm::vec3 velocity;
    float radius;
};

struct PaddleComponent{};
struct BrickComponent{};
struct ScoreComponent{ int value=0; };
struct LivesComponent{ int value=3; };

class EntityManager {
public:
    std::unordered_map<EntityID, TransformComponent> transforms;
    std::unordered_map<EntityID, RenderComponent> renders;
    std::unordered_map<EntityID, BallComponent> balls;
    std::unordered_map<EntityID, PaddleComponent> paddles;
    std::unordered_map<EntityID, BrickComponent> bricks;
    std::unordered_map<EntityID, ScoreComponent> scores;
    std::unordered_map<EntityID, LivesComponent> lives;

    std::vector<EntityID> entities;
    EntityID nextID = 0;

    EntityID createEntity() {
        EntityID id = nextID++;
        entities.push_back(id);
        transforms[id] = TransformComponent{};
        renders[id] = RenderComponent{};
        return id;
    }

    void destroyEntity(EntityID id){
        entities.erase(std::remove(entities.begin(), entities.end(), id), entities.end());
        transforms.erase(id);
        renders.erase(id);
        balls.erase(id);
        paddles.erase(id);
        bricks.erase(id);
        scores.erase(id);
        lives.erase(id);
    }

    void reset(){
        entities.clear();
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

// ---------------- Mesh helpers ----------------
struct Mesh{ GLuint vao=0; GLuint vbo=0; GLuint ebo=0; GLsizei indexCount=0; };

Mesh makeCube(){
    Mesh m;
    float vertices[]={
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,  -0.5f,0.5f,-0.5f,
        -0.5f,-0.5f,0.5f,   0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f
    };
    unsigned int indices[]={
        0,1,2,2,3,0,  4,5,6,6,7,4,
        0,1,5,5,4,0,  2,3,7,7,6,2,
        0,3,7,7,4,0,  1,2,6,6,5,1
    };
    m.indexCount = 36;
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return m;
}

Mesh makeSphere(int latSeg=16,int longSeg=16,float radius=0.5f){
    Mesh m; std::vector<float> verts; std::vector<unsigned int> idx;
    for(int y=0;y<=latSeg;y++){
        float theta = y*M_PI/latSeg;
        float sinTheta=sin(theta), cosTheta=cos(theta);
        for(int x=0;x<=longSeg;x++){
            float phi = x*2*M_PI/longSeg;
            float sinPhi=sin(phi), cosPhi=cos(phi);
            verts.push_back(radius*cosPhi*sinTheta);
            verts.push_back(radius*cosTheta);
            verts.push_back(radius*sinPhi*sinTheta);
        }
    }
    for(int y=0;y<latSeg;y++){
        for(int x=0;x<longSeg;x++){
            int a=y*(longSeg+1)+x;
            int b=a+longSeg+1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a+1);
            idx.push_back(b); idx.push_back(b+1); idx.push_back(a+1);
        }
    }
    m.indexCount=(GLsizei)idx.size();
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),idx.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return m;
}

// ---------------- Shader helpers ----------------
void checkShaderCompile(GLuint shader,const char* name){
    GLint ok; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){ char buf[1024]; glGetShaderInfoLog(shader,1024,NULL,buf);
        std::cerr<<"Shader "<<name<<" compile error:\n"<<buf<<std::endl; }
}
void checkProgramLink(GLuint prog,const char* name){
    GLint ok; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if(!ok){ char buf[1024]; glGetProgramInfoLog(prog,1024,NULL,buf);
        std::cerr<<"Program "<<name<<" link error:\n"<<buf<<std::endl; }
}

// ---------------- Collision ----------------
bool aabbCollision2D(const glm::vec3& boxPos,const glm::vec3& boxScale,
                     const glm::vec3& ballPos,float radius){
    return (ballPos.x+radius>boxPos.x-boxScale.x/2.0f &&
            ballPos.x-radius<boxPos.x+boxScale.x/2.0f &&
            ballPos.y+radius>boxPos.y-boxScale.y/2.0f &&
            ballPos.y-radius<boxPos.y+boxScale.y/2.0f);
}

// ---------------- Shader sources ----------------
const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
void main(){ FragPos=vec3(model*vec4(aPos,1.0)); gl_Position=projection*view*vec4(FragPos,1.0);}
)glsl";

const char* fragmentShaderSrc = R"glsl(
#version 330 core
in vec3 FragPos;
uniform vec4 uColor;
out vec4 FragColor;
void main(){
    vec3 lightDir=normalize(vec3(1.0,1.0,2.0));
    float diff=max(dot(normalize(FragPos),lightDir),0.0);
    vec3 color=uColor.rgb*(0.3+0.7*diff);
    FragColor=vec4(color,uColor.a);
}
)glsl";

// ---------------- Systems ----------------
void InputSystem(EntityManager& em, GLFWwindow* window, float dt){
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // WINDOW SIZE
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Convert mouse screen X to world X (-15..15)
    // screen 0 -> left, width -> right
    float worldX = (float(mouseX) / float(width)) * 30.0f - 15.0f;

    for(auto &p : em.paddles){
        EntityID id = p.first;
        auto& t = em.transforms[id];

        float speed = 20.0f;

        // --- KEYBOARD CONTROL ---
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            t.pos.x -= speed * dt;

        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            t.pos.x += speed * dt;

        // --- MOUSE CONTROL ---
        // Smooth follow (prevents jitter)
        float smooth = 10.0f;
        t.pos.x += (worldX - t.pos.x) * smooth * dt;

        // --- CLAMP PADDLE INSIDE BOUNDS ---
        if(t.pos.x - t.scale.x/2.0f < -15.0f)
            t.pos.x = -15.0f + t.scale.x/2.0f;

        if(t.pos.x + t.scale.x/2.0f > 15.0f)
            t.pos.x = 15.0f - t.scale.x/2.0f;
    }
}

void UISystem(EntityManager& em, bool gameOver, bool& restartRequested)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // --- Info window ---
    ImGui::Begin("Game Info");
    for (auto &kv : em.scores){
        ImGui::Text("Score: %d", kv.second.value);
    }
    for (auto &kv : em.lives){
        ImGui::Text("Lives: %d", kv.second.value);
    }
    ImGui::End();

    // --- Game Over popup (open once) ---
    if (gameOver && !ImGui::IsPopupOpen("Game Over!")) {
        ImGui::OpenPopup("Game Over!");
    }

    if (ImGui::BeginPopupModal("Game Over!", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("You lost all your lives!");
        ImGui::Separator();

        if (ImGui::Button("Restart"))
        {
            restartRequested = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


class PhysicsSystem {
public:
    EntityManager& em;
    glm::vec3 paddleStartPos;
    glm::vec3 ballStartPos;
    glm::vec3 ballStartVelocity;

    PhysicsSystem(EntityManager& manager, const glm::vec3& paddlePos,
                  const glm::vec3& ballPos, const glm::vec3& ballVel)
        : em(manager), paddleStartPos(paddlePos), ballStartPos(ballPos), ballStartVelocity(ballVel) {}

    // update returns true if game should continue, false => game over
    bool update(float dt) {
        bool resetNeeded = false;

        for(auto &kv : em.balls){
            EntityID bid = kv.first;
            auto& ballT = em.transforms[bid];
            auto& ballC = em.balls[bid];

            // movement
            ballT.pos += ballC.velocity * dt;
            ballT.pos.z = 0.0f;

            // walls
            if(ballT.pos.x - ballC.radius < -15.0f){
                ballT.pos.x = -15.0f + ballC.radius;
                ballC.velocity.x *= -1;
                std::cout << "Ball hit left wall\n";
            }
            if(ballT.pos.x + ballC.radius > 15.0f){
                ballT.pos.x = 15.0f - ballC.radius;
                ballC.velocity.x *= -1;
                std::cout << "Ball hit right wall\n";
            }
            if(ballT.pos.y + ballC.radius > 15.0f){
                ballT.pos.y = 15.0f - ballC.radius;
                ballC.velocity.y *= -1;
                std::cout << "Ball hit top wall\n";
            }

            // paddle collision
            for(auto &pp : em.paddles){
                EntityID pid = pp.first;
                auto& paddleT = em.transforms[pid];
                if(aabbCollision2D(paddleT.pos, paddleT.scale, ballT.pos, ballC.radius)){
                    float hit = (ballT.pos.x - paddleT.pos.x) / (paddleT.scale.x / 2.0f);
                    hit = glm::clamp(hit,-1.0f,1.0f);
                    float speed = glm::length(glm::vec2(ballC.velocity.x, ballC.velocity.y));
                    float angle = hit * glm::radians(45.0f);
                    ballC.velocity.x = speed * sin(angle);
                    ballC.velocity.y = speed * cos(angle);
                    ballT.pos.y = paddleT.pos.y + paddleT.scale.y / 2.0f + ballC.radius;
                    std::cout << "Ball hit paddle\n";
                }
            }

            // bricks
            std::vector<EntityID> destroyed;
            for(auto &bb : em.bricks){
                EntityID brickID = bb.first;
                auto& brickT = em.transforms[brickID];
                if(aabbCollision2D(brickT.pos, brickT.scale, ballT.pos, ballC.radius)){
                    glm::vec2 dir = glm::vec2(ballT.pos.x - brickT.pos.x, ballT.pos.y - brickT.pos.y);
                    if(std::abs(dir.x) > std::abs(dir.y)) ballC.velocity.x *= -1;
                    else ballC.velocity.y *= -1;
                    destroyed.push_back(brickID);
                }
            }

            for(auto b : destroyed){
                em.destroyEntity(b);
                for(auto &sd : em.scores){
                    sd.second.value += 10;
                    std::cout << "Ball hit brick " << b << " -> Score: " << sd.second.value << "\n";
                }
            }

            // fell
            if(ballT.pos.y - ballC.radius < -10.0f){
                for(auto &lv : em.lives){
                    lv.second.value--;
                    std::cout << "Life lost! Lives left: " << lv.second.value << "\n";
                }
                resetNeeded = true;
                break;
            }
        }

        if(resetNeeded) resetBallAndPaddle();

        // check lives
        for(auto &lv : em.lives){
            if(lv.second.value <= 0) return false;
        }
        return true;
    }

private:
    void resetBallAndPaddle(){
        for(auto &pp : em.paddles) em.transforms[pp.first].pos = paddleStartPos;
        for(auto &bb : em.balls){
            em.transforms[bb.first].pos = ballStartPos;
            em.balls[bb.first].velocity = ballStartVelocity;
        }
    }
};


// ---------------- Render System ----------------
void RenderSystem(EntityManager& em, GLuint program){
    glClearColor(0.1f,0.1f,0.15f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::mat4 view = glm::lookAt(cameraPos,cameraPos+cameraFront,cameraUp);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f);
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program,"projection"),1,GL_FALSE,glm::value_ptr(projection));

    for(auto id : em.entities){
        auto& t = em.transforms[id];
        auto& r = em.renders[id];
        glm::mat4 model = glm::translate(glm::mat4(1.0f),t.pos)
                          * glm::rotate(glm::mat4(1.0f),t.angle,t.rotationAxis)
                          * glm::scale(glm::mat4(1.0f),t.scale);
        glUniformMatrix4fv(glGetUniformLocation(program,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniform4fv(glGetUniformLocation(program,"uColor"),1,glm::value_ptr(r.color));
        glBindVertexArray(r.vao);
        glDrawElements(GL_TRIANGLES,r.indexCount,GL_UNSIGNED_INT,0);
    }
}

// ---------------- Helpers to (re)create game ----------------
void setupGame(EntityManager &em, Mesh &cubeMesh, Mesh &ballMesh, EntityID &outPaddle, EntityID &outBall){
    em.reset();

    // Paddle
    outPaddle = em.createEntity();
    em.transforms[outPaddle].pos = glm::vec3(0,-5,0);
    em.transforms[outPaddle].scale = glm::vec3(6,1,2);
    em.renders[outPaddle].vao = cubeMesh.vao; em.renders[outPaddle].indexCount = cubeMesh.indexCount;
    em.renders[outPaddle].color = glm::vec4(0.3f,0.8f,0.3f,1.0f);
    em.paddles[outPaddle] = PaddleComponent{};

    // Bricks
    int rows = 5, cols = 10;
    for(int r=0;r<rows;r++){
        for(int c=0;c<cols;c++){
            EntityID b = em.createEntity();
            em.transforms[b].pos = glm::vec3(c*3-13.5f, r*2+2, 0);
            em.transforms[b].scale = glm::vec3(2.5f,1.0f,1.0f);
            em.renders[b].vao = cubeMesh.vao; em.renders[b].indexCount = cubeMesh.indexCount;
            em.renders[b].color = glm::vec4(float(rand())/RAND_MAX,float(rand())/RAND_MAX,float(rand())/RAND_MAX,1.0f);
            em.bricks[b] = BrickComponent{};
        }
    }

    // Ball
    outBall = em.createEntity();
    em.transforms[outBall].pos = glm::vec3(0,-3,0);
    em.transforms[outBall].scale = glm::vec3(1.0f);
    em.renders[outBall].vao = ballMesh.vao; em.renders[outBall].indexCount = ballMesh.indexCount;
    em.renders[outBall].color = glm::vec4(1,0,0,1);
    em.balls[outBall] = BallComponent{glm::vec3(5,8,0),0.5f};

    // Player score & lives
    EntityID player = em.createEntity();
    em.scores[player] = ScoreComponent{0};
    em.lives[player] = LivesComponent{3};
}

// ---------------- Main ----------------
int main(){
    srand((unsigned int)time(0));
    if(!glfwInit()){
        std::cerr<<"Failed to init GLFW\n"; return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window=glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"3D ECS Arkanoid",NULL,NULL);
    if(!window){ std::cerr<<"Failed window\n"; glfwTerminate(); return -1;}
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"Failed GLAD\n"; return -1;}
    glEnable(GL_DEPTH_TEST);

    // compile shader
    GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&vertexShaderSrc,NULL); glCompileShader(vs); checkShaderCompile(vs,"vertex");
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&fragmentShaderSrc,NULL); glCompileShader(fs); checkShaderCompile(fs,"fragment");
    GLuint program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program); checkProgramLink(program,"program"); glDeleteShader(vs); glDeleteShader(fs);

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // ECS + meshes + initial setup
    EntityManager em;
    Mesh cubeMesh = makeCube();
    Mesh ballMesh = makeSphere(12,12,0.5f);

    EntityID paddle = 0, ball = 0;
    setupGame(em, cubeMesh, ballMesh, paddle, ball);

    // physics system (holds start positions)
    PhysicsSystem physics(em, em.transforms[paddle].pos, em.transforms[ball].pos, em.balls[ball].velocity);

    float lastTime = (float)glfwGetTime();
    bool gameRunning = true;
    bool restartRequested = false;
    float restartCooldown = 0.0f;

    while(!glfwWindowShouldClose(window)){
        float current = (float)glfwGetTime();
        float dt = current - lastTime;
        lastTime = current;

        glfwPollEvents();

        if(glfwGetKey(window,GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        // Input
        InputSystem(em, window, dt);

        // Physics: only when gameRunning and cooldown expired
        if(gameRunning){
            if(restartCooldown > 0.0f){
                restartCooldown -= dt;
            }else{
                gameRunning = physics.update(dt); // returns false on game over
            }
        }

        // Render world first
        RenderSystem(em, program);

        // UI: pass gameOver flag = !gameRunning
        UISystem(em, !gameRunning, restartRequested);

        // Handle restart AFTER UI render (important!)
        if(restartRequested){
            // recreate entire game state
            setupGame(em, cubeMesh, ballMesh, paddle, ball);

            // update physics object with new starts
            physics.update(dt);

            // reset control flags
            gameRunning = true;
            restartRequested = false;

            // grace period so physics won't immediately flip gameRunning back to false
            restartCooldown = 0.3f;

            std::cout << "Game Restarted!\n";
        }

        // swap
        glfwSwapBuffers(window);
    }

    // cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

