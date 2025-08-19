#ifndef CUBES_H
#define CUBES_H
#define SD_ENABLE_IRRKLANG 1

#include <iostream>
#include <chrono>
#include <thread>
#include "../lib/glad/include/glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "glfw/glfw3.h"
#include "../lib/ThreadPool.h"
#include "../lib/stb/image.h"

// #include "model.h"
// #include "player_model.h"
// #include "spritesheet.h"
// #include "enemy_spawner.h"
// #include "geom.h"
// #include "capsule.h"
// #include "enemy.h"
// #include "bullet_store.h"
// Build deps: GLFW, GLAD, GLM (header-only)
// C++17
// Example build (Linux/macOS):
//   g++ -std=c++17 main.cpp -o cubes \
//       -lglfw -ldl -framework Cocoa -framework IOKit -framework CoreVideo  // macOS
// Linux: g++ -std=c++17 main.cpp -o cubes -lglfw -ldl -lX11 -lpthread -lXrandr -lXi
// Windows (MinGW): g++ -std=c++17 main.cpp -o cubes -lglfw3 -lopengl32 -lgdi32
// Make sure to compile GLAD as C and include glad.c in your build, or use a prebuilt glad.



#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ------------------------------
// Simple shader helpers
// ------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok){
        GLint len = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << std::endl;
        exit(EXIT_FAILURE);
    }
    return sh;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok){
        GLint len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << std::endl;
        exit(EXIT_FAILURE);
    }
    return prog;
}

// ------------------------------
// Shaders (OpenGL 3.3 core)
// - Camera UBO at binding 0 contains view & proj matrices
// - Per-instance model matrix via instance VBO (attribs 2..5 with divisor=1)
// ------------------------------
static const char* kVS = R"GLSL(
#version 330 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec2 aUV;
layout (location=2) in mat4 iModel; // uses 2,3,4,5 locations

layout(std140) uniform Camera {
    mat4 uView;
    mat4 uProj;
};

out vec2 vUV;

void main(){
    vUV = aUV;
    gl_Position = uProj * uView * iModel * vec4(aPos, 1.0);
}
)GLSL";

//=============================================================


static const char* kFS = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;

void main(){
    vec3 base = texture(uTex, vUV).rgb;
    // simple gamma-correct output
    FragColor = vec4(pow(base, vec3(1.0/2.2)), 1.0);
}
)GLSL";

// ------------------------------
// Geometry: unit cube with UVs
// ------------------------------
struct Vertex { glm::vec3 pos; glm::vec2 uv; };

static void makeCube(std::vector<Vertex>& verts, std::vector<unsigned>& idx){
    // 24 unique vertices (each face has separate UVs)
    const glm::vec3 p[8] = {
        {-0.5f,-0.5f,-0.5f}, {+0.5f,-0.5f,-0.5f}, {+0.5f,+0.5f,-0.5f}, {-0.5f,+0.5f,-0.5f},
        {-0.5f,-0.5f,+0.5f}, {+0.5f,-0.5f,+0.5f}, {+0.5f,+0.5f,+0.5f}, {-0.5f,+0.5f,+0.5f}
    };
    auto face = [&](int a, int b, int c, int d){
        size_t base = verts.size();
        verts.push_back({p[a], {0,0}});
        verts.push_back({p[b], {1,0}});
        verts.push_back({p[c], {1,1}});
        verts.push_back({p[d], {0,1}});
        idx.insert(idx.end(), { (unsigned)base+0, (unsigned)base+1, (unsigned)base+2,
                               (unsigned)base+0, (unsigned)base+2, (unsigned)base+3 });
    };
    face(0,1,2,3); // back
    face(4,5,6,7); // front
    face(0,4,7,3); // left
    face(1,5,6,2); // right
    face(3,2,6,7); // top
    face(0,1,5,4); // bottom
}

// ------------------------------
// Procedural checkerboard texture (no external files needed)
// ------------------------------
static GLuint makeCheckerTex(int size=256, int checks=8){
    std::vector<unsigned char> data(size*size*3);
    for(int y=0;y<size;y++){
        for(int x=0;x<size;x++){
            int cx = (x * checks) / size;
            int cy = (y * checks) / size;
            bool odd = ((cx + cy) & 1) != 0;
            unsigned char v = odd ? 230 : 40;
            data[(y*size + x)*3 + 0] = v;
            data[(y*size + x)*3 + 1] = odd ? 80 : 200;
            data[(y*size + x)*3 + 2] = odd ? 80 : 240;
        }
    }
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

// ------------------------------
// Main
// ------------------------------
int main(){
    if(!glfwInit()){
        std::cerr << "Failed to init GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* win = glfwCreateWindow(1280, 720, "6 Rotujících krychlí (GLFW + UBO + instancing)", nullptr, nullptr);
    if(!win){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "Failed to init GLAD" << std::endl;
        return -1;
    }

    // Compile shaders
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFS);
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    // Geometry buffers: VAO + VBO + IBO
    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    makeCube(verts, idx);

    GLuint vao=0, vbo=0, ebo=0;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1,&ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,pos));
    glEnableVertexAttribArray(1); // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,uv));

    // Instance buffer with model matrices (6 instances)
    const int kInstances = 6;
    GLuint instanceVBO=0; glGenBuffers(1,&instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, kInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    // set attribute pointers for matrix (4 vec4 attributes)
    std::size_t vec4Size = sizeof(glm::vec4);
    for(int i=0;i<4;i++){
        glEnableVertexAttribArray(2+i);
        glVertexAttribPointer(2+i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i*vec4Size));
        glVertexAttribDivisor(2+i, 1);
    }

    glBindVertexArray(0);

    // Camera UBO (binding point 0)
    GLuint cameraUBO=0; glGenBuffers(1,&cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4)*2, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);

    // Link program uniform block to binding 0
    GLuint camIndex = glGetUniformBlockIndex(prog, "Camera");
    glUniformBlockBinding(prog, camIndex, 0);

    // Texture
    GLuint tex = makeCheckerTex();

    // State
    glEnable(GL_DEPTH_TEST);

    auto start = std::chrono::high_resolution_clock::now();

    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();

        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0,0,w,h);
        glClearColor(0.06f,0.07f,0.09f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // time
        auto now = std::chrono::high_resolution_clock::now();
        float t = std::chrono::duration<float>(now - start).count();

        // Camera matrices -> UBO
        glm::mat4 view = glm::lookAt(glm::vec3(3.5f, 2.5f, 6.0f), glm::vec3(0.0f), glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)w/(float)h, 0.1f, 100.0f);
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));

        // Update per-instance model matrices (6 cubes in a circle)
        std::array<glm::mat4, kInstances> models;


        for(int i=0;i<kInstances;i++){
            float angle = i * (glm::two_pi<float>() / kInstances) + t*0.6f;
            float radius = 3.0f;
            glm::vec3 pos = { std::cos(angle)*radius, (i-2.5f)*0.25f, std::sin(angle)*radius };
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            M = glm::rotate(M, t*1.3f + i*0.35f, glm::normalize(glm::vec3(0.3f*i+0.5f, 1.0f, 0.0f)));
            M = glm::scale(M, glm::vec3(0.9f));
            models[i] = M;
        }
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(models), models.data());

        // Draw
        glUseProgram(prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        GLint samplerLoc = glGetUniformLocation(prog, "uTex");
        glUniform1i(samplerLoc, 0);

        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0, kInstances);

        glfwSwapBuffers(win);
    }

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &cameraUBO);
    glDeleteBuffers(1, &instanceVBO);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

#endif // CUBES_H
