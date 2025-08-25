#ifndef LIGHTS_H
#define LIGHTS_H
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


// ------------------------------
//
// Vylepšení: FPS kamera (WASD + myš), Blinn-Phong osvětlení, normály, toggly
// ------------------------------
// ------------------------------------------------------------
// 6 instancovaných texturovaných krychlí (GLFW + GLAD + GLM)
// Nově: načítání textury (stb_image), více světel (UBO), sRGB pipeline
// Ovládání: WASD + Q/E (nahoru/dolů), myš (otáčení), Shift (sprint),
//           P (pauza rotací), F1 (wireframe), Esc (konec)
// ------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <chrono>


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// ------------------------------
// Pomocné funkce shaderů
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
        std::exit(EXIT_FAILURE);
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
        std::exit(EXIT_FAILURE);
    }
    return prog;
}

// ------------------------------
// GLSL shadery (OpenGL 3.3 core)
// - UBO "Camera" (binding 0): view, proj
// - UBO "Lights" (binding 1): pole světel (pozice+barva+intenzita)
// - Instancing: iModel (mat4) v locations 3..6 (kvůli normálám je posun)
// ------------------------------
static const char* kVS = R"GLSL(
#version 330 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec2 aUV;
layout (location=2) in vec3 aNormal;
layout (location=3) in mat4 iModel;   // zabere 3,4,5,6

layout(std140) uniform Camera {
    mat4 uView;
    mat4 uProj;
};

out vec2 vUV;
out vec3 vWorldPos;
out vec3 vWorldNormal;

void main(){
    // správná transformace normálové matice
    mat3 normalMat = mat3(transpose(inverse(iModel)));
    vec4 worldPos = iModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize(normalMat * aNormal);
    vUV = aUV;
    gl_Position = uProj * uView * worldPos;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec3 vWorldPos;
in vec3 vWorldNormal;
out vec4 FragColor;

uniform sampler2D uTex;   // sRGB textura (GL_SRGB8/GL_SRGB8_ALPHA8)
uniform vec3 uViewPos;    // pozice kamery (svět. souř.)

// Definice jednoho světla ve std140-friendly podobě
// pos.xyz = pozice point lightu, pos.w = ignorováno (rezerva)
// color.rgb = barva, color.a = intenzita (škálování)
struct Light {
    vec4 pos;
    vec4 color;
};

// UBO s polem světel (max 8) + počet světel v ivec4.x (std140 zarovnání)
layout(std140) uniform Lights {
    Light uLights[8];
    ivec4 uCount;      // uCount.x = počet světel
};

void main(){
    // V sRGB pipeline textury ukládáme jako sRGB a framebuffer je sRGB,
    // takže zde pracujeme už v LINEÁRU bez ruční gamma konverze.
    vec3 base = texture(uTex, vUV).rgb;

    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uViewPos - vWorldPos);

    // Globální ambient (mírný základ)
    vec3 color = 0.10 * base;

    // Blinn-Phong pro každé světlo (bez atenuace kvůli jednoduchosti)
    for (int i = 0; i < uCount.x; ++i) {
        vec3 L = normalize(uLights[i].pos.xyz - vWorldPos);
        vec3 H = normalize(L + V);

        float NdotL = max(dot(N, L), 0.0);
        float spec  = (NdotL > 0.0) ? pow(max(dot(N, H), 0.0), 32.0) : 0.0;

        vec3 lightCol = uLights[i].color.rgb * uLights[i].color.a; // barva * intenzita
        vec3 diffuse  = base * lightCol * NdotL;
        vec3 specular = 0.25 * lightCol * spec;

        color += diffuse + specular;
    }

    // sRGB framebuffer provede lineár->sRGB sám
    FragColor = vec4(color, 1.0);
}
)GLSL";

// ------------------------------
// Geometrie: krychle s UV + normálami (po stěnách)
// ------------------------------
struct Vertex { glm::vec3 pos; glm::vec2 uv; glm::vec3 nrm; };

static void pushFace(std::vector<Vertex>& v, std::vector<unsigned>& id,
                     glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                     glm::vec3 n){
    size_t base = v.size();
    v.push_back({a,{0,0},n});
    v.push_back({b,{1,0},n});
    v.push_back({c,{1,1},n});
    v.push_back({d,{0,1},n});
    id.insert(id.end(), { (unsigned)base+0,(unsigned)base+1,(unsigned)base+2,
                         (unsigned)base+0,(unsigned)base+2,(unsigned)base+3 });
}

static void makeCube(std::vector<Vertex>& verts, std::vector<unsigned>& idx){
    const glm::vec3 p[8] = {
        {-0.5f,-0.5f,-0.5f}, {+0.5f,-0.5f,-0.5f}, {+0.5f,+0.5f,-0.5f}, {-0.5f,+0.5f,-0.5f},
        {-0.5f,-0.5f,+0.5f}, {+0.5f,-0.5f,+0.5f}, {+0.5f,+0.5f,+0.5f}, {-0.5f,+0.5f,+0.5f}
    };
    pushFace(verts, idx, p[0],p[1],p[2],p[3], {0,0,-1}); // back
    pushFace(verts, idx, p[4],p[5],p[6],p[7], {0,0,+1}); // front
    pushFace(verts, idx, p[0],p[4],p[7],p[3], {-1,0,0}); // left
    pushFace(verts, idx, p[1],p[5],p[6],p[2], {+1,0,0}); // right
    pushFace(verts, idx, p[3],p[2],p[6],p[7], {0,+1,0}); // top
    pushFace(verts, idx, p[0],p[1],p[5],p[4], {0,-1,0}); // bottom
}

// ------------------------------
// Načtení sRGB textury přes stb_image
// - pokud obrázek má 3 kanály -> GL_SRGB8
// - pokud 4 kanály -> GL_SRGB8_ALPHA8
// ------------------------------
static GLuint loadTextureSRGB(const char* path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        std::exit(1);
    }

    GLenum src = (ch == 4) ? GL_RGBA : GL_RGB;
    GLenum internal = (ch == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;

    GLuint tex=0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, src, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

// ------------------------------
// FPS kamera a vstup
// ------------------------------
struct Camera {
    glm::vec3 pos{0.0f, 1.8f, 8.0f};
    float yaw = -90.0f;
    float pitch = -10.0f;
    float fov = 60.0f;
    float speed = 4.2f;
    float sensitivity = 0.12f;
    bool firstMouse = true;
    double lastX = 0.0, lastY = 0.0;

    glm::vec3 front() const {
        float cy = cos(glm::radians(yaw)), sy = sin(glm::radians(yaw));
        float cp = cos(glm::radians(pitch)), sp = sin(glm::radians(pitch));
        return glm::normalize(glm::vec3(cy*cp, sp, sy*cp));
    }
    glm::mat4 view() const {
        return glm::lookAt(pos, pos + front(), {0,1,0});
    }
};
static Camera gCam;
static bool gPaused = false;
static bool gWire = false;

static void mouse_cb(GLFWwindow*, double xpos, double ypos){
    if(gCam.firstMouse){ gCam.lastX = xpos; gCam.lastY = ypos; gCam.firstMouse = false; }
    double dx = xpos - gCam.lastX;
    double dy = gCam.lastY - ypos;
    gCam.lastX = xpos; gCam.lastY = ypos;

    gCam.yaw   += (float)dx * gCam.sensitivity;
    gCam.pitch += (float)dy * gCam.sensitivity;
    gCam.pitch = glm::clamp(gCam.pitch, -89.0f, 89.0f);
}
static void key_cb(GLFWwindow* w, int key, int, int action, int){
    if(action == GLFW_PRESS){
        if(key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
        if(key == GLFW_KEY_P) gPaused = !gPaused;
        if(key == GLFW_KEY_F1){ gWire = !gWire; glPolygonMode(GL_FRONT_AND_BACK, gWire?GL_LINE:GL_FILL); }
    }
}

// ------------------------------
// Struktury pro UBO se světly (std140)
// ------------------------------
struct GPULight { glm::vec4 pos; glm::vec4 color; }; // stejné jako GLSL Light
constexpr int MAX_LIGHTS = 8;

// ------------------------------
// Hlavní
// ------------------------------
int main(){
    // GLFW + okno
    if(!glfwInit()){ std::cerr << "Failed to init GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Instancing + UBO + multi-lights + sRGB", nullptr, nullptr);
    if(!win){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "Failed to init GLAD\n"; return -1;
    }

    glfwSetCursorPosCallback(win, mouse_cb);
    glfwSetKeyCallback(win, key_cb);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Shadery
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFS);
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    // Geometrie
    std::vector<Vertex> verts; std::vector<unsigned> idx;
    makeCube(verts, idx);

    GLuint vao=0, vbo=0, ebo=0;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1,&ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

    // Atributy: pos(0), uv(1), normal(2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,nrm));

    // Instancing buffer: model matice (mat4 => 4x vec4) přes locations 3..6
    const int kInstances = 6;
    GLuint instanceVBO=0;
    glGenBuffers(1,&instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, kInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    std::size_t vec4Size = sizeof(glm::vec4);
    for(int i=0;i<4;i++){
        glEnableVertexAttribArray(3+i);
        glVertexAttribPointer(3+i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i*vec4Size));
        glVertexAttribDivisor(3+i, 1);
    }
    glBindVertexArray(0);

    // UBO: kamera (binding 0)
    GLuint cameraUBO=0; glGenBuffers(1,&cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4)*2, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);
    GLuint camIndex = glGetUniformBlockIndex(prog, "Camera");
    glUniformBlockBinding(prog, camIndex, 0);

    // UBO: světla (binding 1) — pole Light + ivec4 count
    GLuint lightsUBO=0; glGenBuffers(1, &lightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    GLsizeiptr lightsSize = sizeof(GPULight) * MAX_LIGHTS + sizeof(glm::ivec4);
    glBufferData(GL_UNIFORM_BUFFER, lightsSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightsUBO);
    GLuint lightsIndex = glGetUniformBlockIndex(prog, "Lights");
    glUniformBlockBinding(prog, lightsIndex, 1);

    // Textura (sRGB)
    GLuint tex = loadTextureSRGB("floor.png"); // <- uprav dle své cesty

    // Stav: hloubka + sRGB framebuffer
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB); // sRGB konverze při výstupu

    // Inicializace světel (pozice budou trochu animované)
    int lightCount = 4;
    std::array<GPULight, MAX_LIGHTS> lights{};
    lights[0] = { {+3.0f,  2.0f, +4.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.2f} };
    lights[1] = { {-3.0f,  2.0f,  0.0f, 1.0f}, {1.0f, 0.2f, 0.2f, 1.0f} };
    lights[2] = { { 0.0f,  4.0f, +2.0f, 1.0f}, {0.2f, 1.0f, 0.2f, 1.0f} };
    lights[3] = { { 0.0f,  0.5f, +6.0f, 1.0f}, {0.2f, 0.4f, 1.0f, 1.0f} };

    auto t0 = std::chrono::high_resolution_clock::now();
    double lastTime = glfwGetTime();

    while(!glfwWindowShouldClose(win)){
        // delta time
        double nowT = glfwGetTime();
        float dt = float(nowT - lastTime);
        lastTime = nowT;

        glfwPollEvents();

        // vstup: WASD + QE
        float spd = gCam.speed * (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS ? 2.2f : 1.0f);
        glm::vec3 f = glm::normalize(gCam.front());
        glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,1,0)));
        glm::vec3 u = glm::vec3(0,1,0);
        if(glfwGetKey(win, GLFW_KEY_W)==GLFW_PRESS) gCam.pos += f * spd * dt;
        if(glfwGetKey(win, GLFW_KEY_S)==GLFW_PRESS) gCam.pos -= f * spd * dt;
        if(glfwGetKey(win, GLFW_KEY_A)==GLFW_PRESS) gCam.pos -= r * spd * dt;
        if(glfwGetKey(win, GLFW_KEY_D)==GLFW_PRESS) gCam.pos += r * spd * dt;
        if(glfwGetKey(win, GLFW_KEY_Q)==GLFW_PRESS) gCam.pos -= u * spd * dt;
        if(glfwGetKey(win, GLFW_KEY_E)==GLFW_PRESS) gCam.pos += u * spd * dt;

        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0,0,w,h);
        glClearColor(0.06f,0.07f,0.09f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // čas animace (pauzovatelný)
        static float t = 0.0f;
        if(!gPaused) t += dt;

        // Kamera -> UBO
        glm::mat4 view = gCam.view();
        glm::mat4 proj = glm::perspective(glm::radians(gCam.fov), (float)w/(float)h, 0.1f, 100.0f);
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(proj));

        // Aktualizace pozic světel (drobné animace, ať to žije)
        auto now = std::chrono::high_resolution_clock::now();
        float T = std::chrono::duration<float>(now - t0).count();
        lights[0].pos.x = 3.0f + std::sin(T*0.7f)*1.0f;
        lights[1].pos.z = std::sin(T*0.9f)*1.5f;
        lights[2].pos.y = 3.5f + std::sin(T*1.3f)*0.6f;
        // světla -> UBO
        glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPULight)*lightCount, lights.data());
        glm::ivec4 cnt(lightCount,0,0,0);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(GPULight)*MAX_LIGHTS, sizeof(glm::ivec4), &cnt);

        // Model matice instancí (6 kostek do kruhu + rotace)
        std::array<glm::mat4, kInstances> models;
        for(int i=0;i<kInstances;i++){
            float angle = i * (glm::two_pi<float>() / kInstances) + t*0.6f;
            float radius = 3.0f;
            glm::vec3 pos = { std::cos(angle)*radius, (i-2.5f)*0.25f, std::sin(angle)*radius };
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            M = glm::rotate(M, t*1.3f + i*0.35f, glm::normalize(glm::vec3(0.3f*i+0.5f, 1.0f, 0.7f)));
            M = glm::scale(M, glm::vec3(0.9f));
            models[i] = M;
        }
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(models), models.data());

        // Draw
        glUseProgram(prog);

        // uniformy závislé na programu (textura + pozice kamery)
        GLint uTexLoc   = glGetUniformLocation(prog, "uTex");
        GLint uViewPosL = glGetUniformLocation(prog, "uViewPos");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(uTexLoc, 0);
        glUniform3fv(uViewPosL, 1, glm::value_ptr(gCam.pos));

        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0, kInstances);

        glfwSwapBuffers(win);
    }

    // Úklid
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &lightsUBO);
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

#endif // LIGHTS_H
