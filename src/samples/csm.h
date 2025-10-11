#ifndef CSM_H
#define CSM_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
//#include <vector>
#include <string>

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
const int CASCADE_COUNT = 3;

// ---------- Shader helpers ----------
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int success; char info[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if(!success){ glGetShaderInfoLog(s,512,NULL,info); std::cerr<<"Shader compile error: "<<info<<std::endl; }
    return s;
}

GLuint createProgram(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    GLuint vsID = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsID = compileShader(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, vsID);
    glAttachShader(p, fsID);
    glLinkProgram(p);
    int success; char info[512];
    glGetProgramiv(p, GL_LINK_STATUS, &success);
    if(!success){ glGetProgramInfoLog(p,512,NULL,info); std::cerr<<"Program link error: "<<info<<std::endl; }
    glDeleteShader(vsID); glDeleteShader(fsID);
    return p;
}

// ---------- Shaders ----------
const char* depthVS = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
void main(){ gl_Position = lightSpaceMatrix * model * vec4(aPos,1.0); }
)glsl";

const char* depthFS = R"glsl(
#version 330 core
void main(){}
)glsl";

const char* sceneVS = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace[3]; // 3 kaskády
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix[3];
void main(){
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    for(int i=0;i<3;i++) FragPosLightSpace[i] = lightSpaceMatrix[i] * vec4(FragPos,1.0);
    gl_Position = projection * view * vec4(FragPos,1.0);
}
)glsl";

const char* sceneFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace[3];
uniform sampler2D shadowMap[3];
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform float cascadeEnds[3];

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir){
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords*0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float closestDepth = texture(shadowMap[0], projCoords.xy).r; // will be overwritten by caller
    float currentDepth = projCoords.z;
    float bias = max(0.005*(1.0-dot(normal,-lightDir)),0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0/textureSize(shadowMap[0],0);
    for(int x=-1;x<=1;x++)
        for(int y=-1;y<=1;y++){
            float pcfDepth = texture(shadowMap[0], projCoords.xy + vec2(x,y)*texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    shadow/=9.0;
    return shadow;
}

void main(){
    vec3 norm = normalize(Normal);
    vec3 light = normalize(-lightDir);
    float diff = max(dot(norm, light),0.0);
    vec3 color = vec3(1.0);

    // vyber kaskádu podle vzdálenosti
    float depth = length(viewPos - FragPos);
    int cascade = 0;
    if(depth < cascadeEnds[0]) cascade=0;
    else if(depth < cascadeEnds[1]) cascade=1;
    else cascade=2;

    // PCF pro vybranou kaskádu
    vec3 projCoords = FragPosLightSpace[cascade].xyz / FragPosLightSpace[cascade].w;
    projCoords = projCoords*0.5+0.5;
    float currentDepth = projCoords.z;
    float shadow=0.0;
    float bias = max(0.005*(1.0-dot(norm,-lightDir)),0.0005);
    vec2 texelSize = 1.0/textureSize(shadowMap[cascade],0);
    for(int x=-1;x<=1;x++)
        for(int y=-1;y<=1;y++){
            float pcfDepth = texture(shadowMap[cascade], projCoords.xy+vec2(x,y)*texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    shadow/=9.0;

    vec3 lighting = (0.2 + (1.0-shadow)*diff) * color;
    FragColor = vec4(lighting,1.0);
}
)glsl";

// ---------- Mesh helpers ----------
struct Mesh{
    GLuint VAO,VBO,EBO;
    GLsizei idxCount;
};

Mesh createCube(){
    float vertices[] = {
        // positions        // normals
        -1,-1,-1, 0,0,-1, 1,-1,-1, 0,0,-1, 1,1,-1, 0,0,-1, -1,1,-1,0,0,-1,
        -1,-1,1, 0,0,1, 1,-1,1, 0,0,1, 1,1,1,0,0,1, -1,1,1,0,0,1,
        -1,-1,-1,-1,0,0, -1,1,-1,-1,0,0, -1,1,1,-1,0,0, -1,-1,1,-1,0,0,
        1,-1,-1,1,0,0, 1,1,-1,1,0,0, 1,1,1,1,0,0, 1,-1,1,1,0,0,
        -1,-1,-1,0,-1,0, -1,-1,1,0,-1,0, 1,-1,1,0,-1,0, 1,-1,-1,0,-1,0,
        -1,1,-1,0,1,0, -1,1,1,0,1,0, 1,1,1,0,1,0, 1,1,-1,0,1,0
    };
    unsigned int indices[] = {0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
    Mesh m;
    glGenVertexArrays(1,&m.VAO);
    glGenBuffers(1,&m.VBO);
    glGenBuffers(1,&m.EBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER,m.VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    m.idxCount = 36;
    return m;
}

Mesh createPlane(float size=50.0f){
    float half = size*0.5f;
    float vertices[] = {-half,0,-half,0,1,0, half,0,-half,0,1,0, half,0,half,0,1,0, -half,0,half,0,1,0};
    unsigned int indices[] = {0,1,2,2,3,0};
    Mesh m;
    glGenVertexArrays(1,&m.VAO);
    glGenBuffers(1,&m.VBO);
    glGenBuffers(1,&m.EBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER,m.VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    m.idxCount = 6;
    return m;
}

// ---------- Camera ----------
struct Camera{
    glm::vec3 Position=glm::vec3(0,3,8);
    float Yaw=-90.f, Pitch=0.f, Speed=5.f, Sensitivity=0.1f;
    glm::vec3 Front() const{
        glm::vec3 f;
        f.x=cos(glm::radians(Yaw))*cos(glm::radians(Pitch));
        f.y=sin(glm::radians(Pitch));
        f.z=sin(glm::radians(Yaw))*cos(glm::radians(Pitch));
        return glm::normalize(f);
    }
    glm::mat4 GetView() const{
        return glm::lookAt(Position, Position+Front(), glm::vec3(0,1,0));
    }
} camera;

bool keys[1024];
float deltaTime=0,lastFrame=0;
double lastX=SCR_WIDTH/2, lastY=SCR_HEIGHT/2;
bool firstMouse=true;

void key_callback(GLFWwindow* w,int k,int s,int a,int m){if(k>=0&&k<1024){if(a==GLFW_PRESS) keys[k]=true;if(a==GLFW_RELEASE) keys[k]=false;}}

void mouse_callback(GLFWwindow* w,double xpos,double ypos){
    if(firstMouse){lastX=xpos;lastY=ypos;firstMouse=false;}
    double xoffset=xpos-lastX, yoffset=lastY-ypos;
    lastX=xpos; lastY=ypos;
    camera.Yaw += xoffset*camera.Sensitivity;
    camera.Pitch += yoffset*camera.Sensitivity;
    if(camera.Pitch>89.f) camera.Pitch=89.f;
    if(camera.Pitch<-89.f) camera.Pitch=-89.f;
}

void processInput(float dt){
    glm::vec3 front=camera.Front();
    glm::vec3 right=glm::normalize(glm::cross(front,glm::vec3(0,1,0)));
    if(keys[GLFW_KEY_W]) camera.Position+=front*dt*camera.Speed;
    if(keys[GLFW_KEY_S]) camera.Position-=front*dt*camera.Speed;
    if(keys[GLFW_KEY_A]) camera.Position-=right*dt*camera.Speed;
    if(keys[GLFW_KEY_D]) camera.Position+=right*dt*camera.Speed;
    if(keys[GLFW_KEY_SPACE]) camera.Position.y+=dt*camera.Speed;
    if(keys[GLFW_KEY_LEFT_CONTROL]) camera.Position.y-=dt*camera.Speed;
}

// ---------- main ----------
int main(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"CSM Demo",NULL,NULL);
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window,mouse_callback);
    glfwSetKeyCallback(window,key_callback);
    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    GLuint depthShader = createProgram(depthVS, depthFS);
    GLuint sceneShader = createProgram(sceneVS, sceneFS);

    Mesh cube=createCube();
    Mesh plane=createPlane(50.0f);

    // ---------- CSM shadow maps ----------
    GLuint depthMapFBO[CASCADE_COUNT], depthMap[CASCADE_COUNT];
    glGenFramebuffers(CASCADE_COUNT, depthMapFBO);
    glGenTextures(CASCADE_COUNT, depthMap);
    for(int i=0;i<CASCADE_COUNT;i++){
        glBindTexture(GL_TEXTURE_2D,depthMap[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,SHADOW_WIDTH,SHADOW_HEIGHT,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        float border[]={1.0,1.0,1.0,1.0};
        glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
        glBindFramebuffer(GL_FRAMEBUFFER,depthMapFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthMap[i],0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) std::cerr<<"CSM FBO error\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f,-1.0f,-0.3f));

    // Cascade split distances
    float cascadeEnds[CASCADE_COUNT] = {10.0f,30.0f,100.0f};

    // ---------- Render loop ----------
    while(!glfwWindowShouldClose(window)){
        float currentFrame=(float)glfwGetTime();
        deltaTime=currentFrame-lastFrame;
        lastFrame=currentFrame;
        processInput(deltaTime);

        // ---------- 1) Render depth maps ----------
        for(int i=0;i<CASCADE_COUNT;i++){
            // light space matrix pro kaskádu
            float nearPlane = (i==0?0.1f:cascadeEnds[i-1]);
            float farPlane = cascadeEnds[i];
            float orthoSize = farPlane*0.5f;
            glm::mat4 lightProjection = glm::ortho(-orthoSize,orthoSize,-orthoSize,orthoSize,nearPlane,farPlane);
            glm::mat4 lightView = glm::lookAt(-lightDir*farPlane*0.5f, glm::vec3(0,0,0), glm::vec3(0,1,0));
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;

            glViewport(0,0,SHADOW_WIDTH,SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER,depthMapFBO[i]);
            glClear(GL_DEPTH_BUFFER_BIT);
            glCullFace(GL_FRONT);
            glUseProgram(depthShader);
            glUniformMatrix4fv(glGetUniformLocation(depthShader,"lightSpaceMatrix"),1,GL_FALSE,glm::value_ptr(lightSpaceMatrix));
            glm::mat4 modelPlane=glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(depthShader,"model"),1,GL_FALSE,glm::value_ptr(modelPlane));
            glBindVertexArray(plane.VAO); glDrawElements(GL_TRIANGLES,plane.idxCount,GL_UNSIGNED_INT,0);
            glm::mat4 modelCube=glm::translate(glm::mat4(1.0f),glm::vec3(0,1,0));
            glUniformMatrix4fv(glGetUniformLocation(depthShader,"model"),1,GL_FALSE,glm::value_ptr(modelCube));
            glBindVertexArray(cube.VAO); glDrawElements(GL_TRIANGLES,cube.idxCount,GL_UNSIGNED_INT,0);
            glCullFace(GL_BACK);
            glBindFramebuffer(GL_FRAMEBUFFER,0);

            // uložíme matrix pro shader
            std::string name = "lightSpaceMatrix[" + std::to_string(i) + "]";
            glUseProgram(sceneShader);
            glUniformMatrix4fv(glGetUniformLocation(sceneShader,name.c_str()),1,GL_FALSE,glm::value_ptr(lightSpaceMatrix));
        }

        // ---------- 2) Render scene ----------
        glViewport(0,0,SCR_WIDTH,SCR_HEIGHT);
        glClearColor(0.1f,0.1f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(sceneShader);
        glm::mat4 proj = glm::perspective(glm::radians(60.f),(float)SCR_WIDTH/SCR_HEIGHT,0.1f,100.0f);
        glm::mat4 view = camera.GetView();
        glUniformMatrix4fv(glGetUniformLocation(sceneShader,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniform3fv(glGetUniformLocation(sceneShader,"lightDir"),1,glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(sceneShader,"viewPos"),1,glm::value_ptr(camera.Position));
        glUniform1fv(glGetUniformLocation(sceneShader,"cascadeEnds"),CASCADE_COUNT,cascadeEnds);

        for(int i=0;i<CASCADE_COUNT;i++){
            glActiveTexture(GL_TEXTURE0+i);
            glBindTexture(GL_TEXTURE_2D,depthMap[i]);
            std::string name = "shadowMap[" + std::to_string(i) + "]";
            glUniform1i(glGetUniformLocation(sceneShader,name.c_str()),i);
        }

        glm::mat4 modelPlane=glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(sceneShader,"model"),1,GL_FALSE,glm::value_ptr(modelPlane));
        glBindVertexArray(plane.VAO); glDrawElements(GL_TRIANGLES,plane.idxCount,GL_UNSIGNED_INT,0);
        glm::mat4 modelCube=glm::translate(glm::mat4(1.0f),glm::vec3(0,1,0));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader,"model"),1,GL_FALSE,glm::value_ptr(modelCube));
        glBindVertexArray(cube.VAO); glDrawElements(GL_TRIANGLES,cube.idxCount,GL_UNSIGNED_INT,0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

#endif // CSM_H
