#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>
std::vector<std::string> faces1
    {
        "skybox2/right.bmp",
        "skybox2/left.bmp",
        "skybox2/top.bmp",
        "skybox2/bottom.bmp",
        "skybox2/front.bmp",
        "skybox2/back.bmp"
    };
// --- Prototypy funkcí ---
// ------------------- main.cpp -------------------


// --- Nastavení ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// === Kamera ===
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

float yaw   = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// === Prototypy ===
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
unsigned int loadCubemap(std::vector<std::string> faces);
unsigned int loadTexture(const char* path);

// --- Shadery ---
// Skybox vertex
const char *skyboxVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 projection;
uniform mat4 view;
void main()
{
    TexCoords = aPos;
    vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)";

// Skybox fragment
const char *skyboxFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;
void main()
{
    FragColor = texture(skybox, TexCoords);
}
)";

// Sun billboard vertex
const char *sunVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 sunPos; // pevná pozice slunce ve světě
uniform float scale;

void main()
{
    // pevné světové osy
    vec3 right = vec3(1.0, 0.0, 0.0);
    vec3 up    = vec3(0.0, 1.0, 0.0);

    vec3 worldPos = sunPos + (aPos.x * right + aPos.y * up) * scale;

    gl_Position = projection * view * vec4(worldPos, 1.0);
    TexCoord = aTexCoord;
}


)";

// Sun billboard fragment
const char *sunFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D sunTexture;

void main()
{
    vec2 center = TexCoord - vec2(0.5);
    float dist = length(center);

    vec4 texColor = texture(sunTexture, TexCoord);

    float glow = exp(-dist * 15.0);
    vec3 glowColor = vec3(1.0, 0.95, 0.6);

    vec3 color = texColor.rgb + glowColor * glow;

    if(texColor.a < 0.1) discard;

    FragColor = vec4(color, 1.0);
}

)";

// === MAIN ===
int main()
{
    // GLFW init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Skybox + FPS kamera + Slunce", NULL, NULL);
    if (window == NULL) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to init GLAD\n"; return -1; }

    glEnable(GL_DEPTH_TEST);

    // === SHADERS ===
    auto compileShader = [](const char* src, GLenum type){
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        return s;
    };

    // Skybox program
    unsigned int skyVS = compileShader(skyboxVertexShaderSource, GL_VERTEX_SHADER);
    unsigned int skyFS = compileShader(skyboxFragmentShaderSource, GL_FRAGMENT_SHADER);
    unsigned int skyProg = glCreateProgram();
    glAttachShader(skyProg, skyVS);
    glAttachShader(skyProg, skyFS);
    glLinkProgram(skyProg);
    glDeleteShader(skyVS); glDeleteShader(skyFS);

    // Sun program
    unsigned int sunVS = compileShader(sunVertexShaderSource, GL_VERTEX_SHADER);
    unsigned int sunFS = compileShader(sunFragmentShaderSource, GL_FRAGMENT_SHADER);
    unsigned int sunProg = glCreateProgram();
    glAttachShader(sunProg, sunVS);
    glAttachShader(sunProg, sunFS);
    glLinkProgram(sunProg);
    glDeleteShader(sunVS); glDeleteShader(sunFS);

    // === Skybox data ===
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };

    unsigned int skyVAO, skyVBO;
    glGenVertexArrays(1, &skyVAO);
    glGenBuffers(1, &skyVBO);
    glBindVertexArray(skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    // === Sun quad ===
    float quadVertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f
    };
    unsigned int sunVAO, sunVBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));

    // === Cubemap load ===
    std::vector<std::string> faces{
        "skybox/right.jpg",
        "skybox/left.jpg",
        "skybox/top.jpg",
        "skybox/bottom.jpg",
        "skybox/front.jpg",
        "skybox/back.jpg"
    };
    unsigned int cubemapTex = loadCubemap(faces1);

    // === Sun texture ===
    unsigned int sunTex = loadTexture("skybox2/sun.png");

    glUseProgram(skyProg);
    glUniform1i(glGetUniformLocation(skyProg,"skybox"),0);

    // === Render loop ===
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);


        // směr slunce (pevný světový směr)

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
       glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.3f, 1.0f, -0.5f));
        float sunDistance = 100.0f;
        glm::vec3 sunPos = -sunDirection * sunDistance;
        glUseProgram(sunProg);
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunProg,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(sunProg,"sunPos"),1,glm::value_ptr(sunPos));
        glUniform1f(glGetUniformLocation(sunProg,"scale"),10.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,sunTex);
        glUniform1i(glGetUniformLocation(sunProg,"sunTexture"),0);
        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES,0,6);
        glDisable(GL_BLEND);
        // --- Draw Skybox ---
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyProg);
        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyProg,"view"),1,GL_FALSE,glm::value_ptr(skyView));
        glUniformMatrix4fv(glGetUniformLocation(skyProg,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        glBindVertexArray(skyVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP,cubemapTex);
        glDrawArrays(GL_TRIANGLES,0,36);
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// === Helper functions ===
void processInput(GLFWwindow *window)
{
    float speed = 2.5f * deltaTime;
    if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS) cameraPos += speed*cameraFront;
    if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS) cameraPos -= speed*cameraFront;
    if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront,cameraUp))*speed;
    if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront,cameraUp))*speed;
    if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(window,true);
}

void mouse_callback(GLFWwindow* window,double xpos,double ypos)
{
    if(firstMouse){ lastX=xpos; lastY=ypos; firstMouse=false; }
    float xoffset = xpos-lastX;
    float yoffset = lastY-ypos;
    lastX=xpos; lastY=ypos;

    float sensitivity=0.1f;
    xoffset*=sensitivity; yoffset*=sensitivity;
    yaw+=xoffset; pitch+=yoffset;
    if(pitch>89.0f) pitch=89.0f;
    if(pitch<-89.0f) pitch=-89.0f;

    glm::vec3 front;
    front.x=cos(glm::radians(yaw))*cos(glm::radians(pitch));
    front.y=sin(glm::radians(pitch));
    front.z=sin(glm::radians(yaw))*cos(glm::radians(pitch));
    cameraFront=glm::normalize(front);
}

void framebuffer_size_callback(GLFWwindow* window,int width,int height)
{ glViewport(0,0,width,height); }

unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int texID; glGenTextures(1,&texID); glBindTexture(GL_TEXTURE_CUBE_MAP,texID);
    int w,h,n;
    stbi_set_flip_vertically_on_load(false);
    for(unsigned int i=0;i<faces.size();i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(),&w,&h,&n,0);
        if(data){
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,data);
            stbi_image_free(data);
        } else { std::cout<<"Cubemap failed: "<<faces[i]<<"\n"; }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    return texID;
}

unsigned int loadTexture(const char* path)
{
    unsigned int tex; glGenTextures(1,&tex);
    int w,h,n; unsigned char* data = stbi_load(path,&w,&h,&n,0);
    if(data){
        GLenum format = n==4?GL_RGBA:GL_RGB;
        glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,format,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    } else { std::cout<<"Texture failed: "<<path<<"\n"; }
    stbi_image_free(data);
    return tex;
}
