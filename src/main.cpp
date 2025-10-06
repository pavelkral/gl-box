#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>



// --- Konstanty ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Kamera ---
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw   = -90.0f; // horizontální úhel (počítá se od -Z směru)
float pitch =  0.0f;  // vertikální úhel
float lastX =  SCR_WIDTH / 2.0f;
float lastY =  SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float fov = 45.0f;

// --- Časování ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Prototypy ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int createShader(const char* vs, const char* fs);
void renderCube();
void renderSphere();
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
// --- Globální VAO ---
unsigned int cubeVAO = 0, cubeVBO = 0;
unsigned int sphereVAO = 0, sphereVBO = 0, sphereEBO = 0;
unsigned int indexCount;
void setUberMaterial(unsigned int shader, int mode, const glm::vec3& color, float alpha,
                     float ior, float fresnelPwr, float reflectStr)
{
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "renderMode"), mode);
    glUniform3fv(glGetUniformLocation(shader, "materialColor"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shader, "alpha"), alpha);

    // Pouze pro Mode 0 (Reflexe/Sklo)
    if (mode == 0) {
        glUniform1f(glGetUniformLocation(shader, "refractionIndex"), ior);
        glUniform1f(glGetUniformLocation(shader, "fresnelPower"), fresnelPwr);
        glUniform1f(glGetUniformLocation(shader, "reflectionStrength"), reflectStr);
    }
}
// --- Shader zdroje ---
const char* equirectToCubemapVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 WorldPos;
uniform mat4 projection;
uniform mat4 view;
void main()
{
    WorldPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
)glsl";

const char* equirectToCubemapFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 WorldPos;
uniform sampler2D equirectangularMap;
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
void main()
{
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    FragColor = vec4(texture(equirectangularMap, uv).rgb, 1.0);
}
)glsl";

const char* skyboxVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    TexCoords = aPos;
    vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)glsl";

const char* skyboxFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube environmentMap;
void main()
{
    vec3 envColor = texture(environmentMap, TexCoords).rgb;
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2));
    FragColor = vec4(envColor, 1.0);
}
)glsl";

const char* cubeVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
out vec3 Normal;
out vec3 Position;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    Normal = mat3(transpose(inverse(model))) * aNormal;
    Position = vec3(model * vec4(aPos,1.0));
    gl_Position = projection * view * vec4(Position,1.0);
}
)glsl";

const char* cubeFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube environmentMap;

// Uber Parametry:
uniform int renderMode;             // 0: Reflexe/Sklo (default), 1: Cista Barva (difuzni), 2: Matny Odraz (vice k PBR)
uniform vec3 materialColor;         // Barva pro difuzni (mode 1) nebo tonovani skla (mode 0)
uniform float alpha;                // Globalni pruhlednost (pro Blending)

// Reflexe/Sklo Parametry (Mode 0):
uniform float refractionIndex;      // Index lomu (1.0/IOR)
uniform float fresnelPower;         // Mocnitel pro Schlickovu aproximaci (typicky 5.0)
uniform float reflectionStrength;   // Sila odrazu (mix)

void main()
{
    vec3 N = normalize(Normal);
    vec3 I = normalize(Position - cameraPos);

    vec3 finalColor = vec3(0.0);
    float finalAlpha = alpha;

    if (renderMode == 1) // CISTA BARVA (Difuzni/Matna)
    {
        // Ignoruje envMap, kresli jen barvu s alpha. Vhodne pro neprustrelny material.
        finalColor = materialColor;
    }
    else // renderMode == 0 (Reflexe/Sklo/Chrom)
    {
        // --- 1. REFRAKCE (Lámání světla) ---
        vec3 refractedRay = refract(I, N, refractionIndex);

        // Zpracovani totalniho odrazu nebo normalizace
        if (dot(refractedRay, refractedRay) == 0.0) {
            refractedRay = reflect(I, N);
        } else {
            refractedRay = normalize(refractedRay);
        }
        vec3 refractedColor = texture(environmentMap, refractedRay).rgb;

        // --- 2. REFLEXE (Odraz) ---
        vec3 reflectedRay = reflect(I, N);
        vec3 reflectedColor = texture(environmentMap, reflectedRay).rgb;

        // --- 3. FRESNELŮV EFEKT (Směs odrazu a lomu) ---
        // Schlickova aproximace
        float R0 = 0.04; // Zakladni odrazivost dielektrika
        float fresnel = R0 + (1.0 - R0) * pow(1.0 - max(0.0, dot(-I, N)), fresnelPower);

        // Uzivatelska kontrola sily odrazu
        fresnel = mix(fresnel, reflectionStrength, 0.5);

        // --- 4. MIX a TÓNOVÁNÍ ---
        // Finalni barva je mix lomené a odražené barvy
        finalColor = mix(refractedColor, reflectedColor, fresnel);

        // Aplikace barvy pro tónování skla/reflexe
        finalColor *= materialColor;
    }
    // else if (renderMode == 2) { ... } // Lze pridat slozitejsi PBR/matnou logiku

    // Tonemapping a Gamma korekce (aplikuje se vzdy)
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, finalAlpha);
}
)glsl";
int main()
{
    // GLFW init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"HDRI Skybox",NULL,NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cout<<"GLAD failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // --- Shadery ---
    unsigned int equirectToCubemapShader = createShader(equirectToCubemapVS,equirectToCubemapFS);
    unsigned int skyboxShader = createShader(skyboxVS,skyboxFS);
    unsigned int cubeShader = createShader(cubeVS,cubeFS);

    glUseProgram(equirectToCubemapShader);
    glUniform1i(glGetUniformLocation(equirectToCubemapShader,"equirectangularMap"),0);
    glUseProgram(skyboxShader);
    glUniform1i(glGetUniformLocation(skyboxShader,"environmentMap"),0);
    glUseProgram(cubeShader);
    glUniform1i(glGetUniformLocation(cubeShader,"environmentMap"),0);

    // --- Načtení HDRI ---
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float* data = stbi_loadf("assets/sky.hdr",&width,&height,&nrComponents,0);
    if(!data){ std::cout<<"Failed to load HDRI\n"; return -1; }
    unsigned int hdrTexture;
    glGenTextures(1,&hdrTexture);
    glBindTexture(GL_TEXTURE_2D,hdrTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,width,height,0,GL_RGB,GL_FLOAT,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);

    // --- Framebuffer pro konverzi ---
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1,&captureFBO);
    glGenRenderbuffers(1,&captureRBO);
    glBindFramebuffer(GL_FRAMEBUFFER,captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER,captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,captureRBO);

    unsigned int envCubemap;
    glGenTextures(1,&envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP,envCubemap);
    for(unsigned int i=0;i<6;i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB16F,512,512,0,GL_RGB,GL_FLOAT,nullptr);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f),1.0f,0.1f,10.0f);
    glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(1,0,0), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1,0,0), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,-1,0), glm::vec3(0,0,-1)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,0,1), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,0,-1), glm::vec3(0,-1,0))
        };

    glUseProgram(equirectToCubemapShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,hdrTexture);
    glViewport(0,0,512,512);
    glBindFramebuffer(GL_FRAMEBUFFER,captureFBO);
    for(unsigned int i=0;i<6;i++)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader,"view"),1,GL_FALSE,glm::value_ptr(captureViews[i]));
        glUniformMatrix4fv(glGetUniformLocation(equirectToCubemapShader,"projection"),1,GL_FALSE,glm::value_ptr(captureProjection));
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,envCubemap,0);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    // --- Hlavní smyčka ---
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH/(float)SCR_HEIGHT,0.1f,100.0f);
        glm::mat4 view = glm::lookAt(cameraPos,cameraPos+cameraFront,cameraUp);
        //glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)
        // --- Koule ---
        glUseProgram(cubeShader);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(cubeShader,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(cubeShader,"cameraPos"),1,glm::value_ptr(cameraPos));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP,envCubemap);

        glm::vec3 glassColor = glm::vec3(0.7f, 0.9f, 1.0f);
        float ior = 1.0f / 1.52f; // Index lomu pro sklo
        float reflection = 0.5f; // Ruční mix s Fresnelovým efektem
        float alpha = 0.7f; // Průhlednost 70%

        setUberMaterial(cubeShader,
                        0,                                  // renderMode: 0 (Reflexe/Sklo)
                        glm::vec3(0.8f, 0.2f, 1.0f),        // materialColor (Tonovani)
                        0.8f,                               // alpha
                        1.0f / 1.52f,                       // IOR (vzduch/sklo)
                        5.0f,                               // fresnelPower
                        0.8f);
        // Chrom: Mode 0, cista barva (1,1,1), neprůhledný, silná reflexe
        setUberMaterial(cubeShader,
                        0,                                  // renderMode: 0 (Reflexe/Sklo)
                        glm::vec3(1.0f, 1.0f, 1.0f),        // materialColor (Bila/Stribrna)
                        1.0f,                               // alpha
                        1.0f,                               // IOR 1.0 (Refrakce pryc = jen odraz)
                        5.0f,                               // fresnelPower
                        1.0f);                              // reflectionStrength (plny odraz)
        setUberMaterial(cubeShader,
                        1,                                  // renderMode: 1 (Cista Barva)
                        glm::vec3(1.0f, 0.2f, 0.2f),        // materialColor (Cervena)
                        1.0f,                               // alpha
                        0.0f, 0.0f, 0.0f);


        renderSphere();

        // --- Skybox ---
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShader);
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP,envCubemap);
        renderCube();
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// --- Pomocné funkce ---
void framebuffer_size_callback(GLFWwindow* window,int width,int height){ glViewport(0,0,width,height); }
void processInput(GLFWwindow *window)
{
    float cameraSpeed = 2.5f * deltaTime; // rychlost pohybu
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

unsigned int createShader(const char* vs,const char* fs)
{
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vertex,1,&vs,NULL); glCompileShader(vertex);
    int success; char info[512]; glGetShaderiv(vertex,GL_COMPILE_STATUS,&success); if(!success){ glGetShaderInfoLog(vertex,512,NULL,info); std::cout<<"Vertex: "<<info<<"\n"; }

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(frag,1,&fs,NULL); glCompileShader(frag);
    glGetShaderiv(frag,GL_COMPILE_STATUS,&success); if(!success){ glGetShaderInfoLog(frag,512,NULL,info); std::cout<<"Fragment: "<<info<<"\n"; }

    unsigned int program = glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,frag); glLinkProgram(program);
    glGetProgramiv(program,GL_LINK_STATUS,&success); if(!success){ glGetProgramInfoLog(program,512,NULL,info); std::cout<<"Link: "<<info<<"\n"; }

    glDeleteShader(vertex); glDeleteShader(frag);
    return program;
}
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if(firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // y-coord obrácená
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f) pitch = 89.0f;
    if(pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}
// --- Cube a Sphere rendering ---
void renderCube()
{
    if(cubeVAO==0)
    {
        // Původní vrcholy jsou v rozsahu [-1.0, 1.0].
        // Vynásobením 5x se rozsah změní na [-5.0, 5.0], což dá celkový rozměr 10x10x10.
        float vertices[] = {
            // ZÁDA
            -5.0f,-5.0f,-5.0f,  5.0f,-5.0f,-5.0f,  5.0f, 5.0f,-5.0f,
            5.0f, 5.0f,-5.0f, -5.0f, 5.0f,-5.0f, -5.0f,-5.0f,-5.0f,
            // PŘEDEK
            -5.0f,-5.0f, 5.0f,  5.0f,-5.0f, 5.0f,  5.0f, 5.0f, 5.0f,
            5.0f, 5.0f, 5.0f, -5.0f, 5.0f, 5.0f, -5.0f,-5.0f, 5.0f,
            // VRCH
            -5.0f, 5.0f,-5.0f, -5.0f, 5.0f, 5.0f,  5.0f, 5.0f, 5.0f,
            5.0f, 5.0f, 5.0f,  5.0f, 5.0f,-5.0f, -5.0f, 5.0f,-5.0f,
            // SPODEK
            -5.0f,-5.0f,-5.0f, -5.0f,-5.0f, 5.0f,  5.0f,-5.0f, 5.0f,
            5.0f,-5.0f, 5.0f,  5.0f,-5.0f,-5.0f, -5.0f,-5.0f,-5.0f,
            // LEVÁ STRANA
            -5.0f, 5.0f, 5.0f, -5.0f, 5.0f,-5.0f, -5.0f,-5.0f,-5.0f,
            -5.0f,-5.0f,-5.0f, -5.0f,-5.0f, 5.0f, -5.0f, 5.0f, 5.0f,
            // PRAVÁ STRANA
            5.0f,-5.0f,-5.0f,  5.0f,-5.0f, 5.0f,  5.0f, 5.0f, 5.0f,
            5.0f, 5.0f, 5.0f,  5.0f, 5.0f,-5.0f,  5.0f,-5.0f,-5.0f
        };
        glGenVertexArrays(1,&cubeVAO);
        glGenBuffers(1,&cubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER,cubeVBO);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES,0,36);
    glBindVertexArray(0);
}
void renderSphere()
{
    float M_PI = 3.141592654;
    if(sphereVAO == 0)
    {
        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        for(unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for(unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSeg = (float)x / X_SEGMENTS;
                float ySeg = (float)y / Y_SEGMENTS;
                float xPos = cos(xSeg * 2.0f * M_PI) * sin(ySeg * M_PI);
                float yPos = cos(ySeg * M_PI);
                float zPos = sin(xSeg * 2.0f * M_PI) * sin(ySeg * M_PI);
                positions.push_back(glm::vec3(xPos, yPos, zPos));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        for(unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            for(unsigned int x = 0; x < X_SEGMENTS; ++x)
            {
                unsigned int a = y * (X_SEGMENTS + 1) + x;
                unsigned int b = (y + 1) * (X_SEGMENTS + 1) + x;
                unsigned int c = (y + 1) * (X_SEGMENTS + 1) + x + 1;
                unsigned int d = y * (X_SEGMENTS + 1) + x + 1;

                indices.push_back(a); indices.push_back(b); indices.push_back(d);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
        indexCount = indices.size();

        std::vector<float> data;
        for(size_t i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            data.push_back(normals[i].x);
            data.push_back(normals[i].y);
            data.push_back(normals[i].z);
        }

        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);

        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
