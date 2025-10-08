#ifndef SPHERE_H
#define SPHERE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <iostream>

class Sphere
{
private:
    unsigned int shaderProgram;
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;

    static const char* vertexShaderSrc;
    static const char* fragmentShaderSrc;

    void initGeometry();
    static unsigned int createShader(const char* vs, const char* fs);

public:
    // Vlastnosti materiálu
    glm::vec3 color;
    float alpha;
    float metallic;
    float roughness;
    float ao;
    float reflectionStrength;
    float transmission; // 0 = neprůhledný, 1 = průhledný (sklo)
    float ior;          // Index lomu světla (Index of Refraction)

    Sphere();
    ~Sphere();

    void setMaterial(const glm::vec3& col, float a, float m, float r, float ambient, float refl = 1.0f, float trans = 0.0f, float indexOfRefraction = 1.52f);

    void draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
              const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir) const;

    void drawForShadow(unsigned int depthShader, const glm::mat4& model, const glm::mat4& lightSpaceMatrix) const;
};

// ---------------- SHADERY ----------------

const char* Sphere::vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 WorldPos;
out vec3 Normal;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    FragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)glsl";

const char* Sphere::fragmentShaderSrc = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform samplerCube environmentMap;
uniform sampler2D shadowMap;

uniform vec3 materialColor;
uniform float alpha;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform float reflectionStrength;
uniform float transmission; // 0.0 = neprůhledný, 1.0 = sklo/průhledný
uniform float ior;          // Index lomu světla

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0; // Max mipmap level pro odrazy

// --- PBR a Utility funkce ---
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.000001);
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - WorldPos);

    vec3 albedo = materialColor;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // --- LOGIKA PRO PRŮHLEDNÉ MATERIÁLY (SKLO) ---
    if (transmission > 0.0)
    {
        float ratio = 1.0 / ior;
        vec3 T = refract(-V, N, ratio);
        vec3 refractedColor = textureLod(environmentMap, T, roughness * MAX_REFLECTION_LOD).rgb;

        vec3 R = reflect(-V, N);
        vec3 reflectedColor = textureLod(environmentMap, R, roughness * MAX_REFLECTION_LOD).rgb;

        vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);

        vec3 color = mix(refractedColor, reflectedColor, F);

        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));

        FragColor = vec4(color, alpha);
        return;
    }

    // --- LOGIKA PRO NEPRŮHLEDNÉ MATERIÁLY (PLAST, KOV, ATD.) ---
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 diffuse = albedo / PI;
    vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);

    float shadow = ShadowCalculation(FragPosLightSpace, N, L);
    //float shadow = 0.0;
    vec3 Lo = (kD * diffuse + specular) * max(dot(N, L), 0.0) * (1.0 - shadow);

    vec3 R = reflect(-V, N);
    vec3 envColor = textureLod(environmentMap, R, roughness * MAX_REFLECTION_LOD).rgb;

    vec3 F_env = fresnelSchlick(max(dot(N, V), 0.0), F0);

    vec3 specular_IBL = envColor * F_env * reflectionStrength;

    vec3 kD_env = vec3(1.0) - F_env;
    kD_env *= 1.0 - metallic;

    vec3 irradiance = vec3(0.03);
    vec3 diffuse_IBL = irradiance * albedo;

    // UPRAVENO: Ambientní IBL složka je nyní také ovlivněna reflectionStrength.
    // Tím se zajistí, že při reflectionStrength=0.0 zmizí i zbytek IBL světla.
    vec3 ambient = kD_env * diffuse_IBL * ao * reflectionStrength; // <-- ZDE JE ZMĚNA

    vec3 color = ambient + Lo + specular_IBL;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, alpha);
}
)glsl";

// ---------------- IMPLEMENTACE ----------------

inline Sphere::Sphere(){
    shaderProgram = createShader(vertexShaderSrc,fragmentShaderSrc);
    initGeometry();
    color = glm::vec3(1.0f);
    alpha = 1.0f;
    metallic = 0.0f;
    roughness = 0.5f;
    ao = 1.0f;
    reflectionStrength = 1.0f; // Lepší výchozí hodnota
    transmission = 0.0f;
    ior = 1.52f;
}

inline Sphere::~Sphere(){
    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteBuffers(1,&EBO);
}

inline void Sphere::initGeometry(){
    const float M_PI = 3.14159265359f;
    const unsigned int X_SEGMENTS = 64, Y_SEGMENTS = 64;
    std::vector<glm::vec3> pos, norm; std::vector<unsigned int> idx;

    for(unsigned int y=0;y<=Y_SEGMENTS;y++){
        for(unsigned int x=0;x<=X_SEGMENTS;x++){
            float xSeg=(float)x/X_SEGMENTS, ySeg=(float)y/Y_SEGMENTS;
            float xPos=cos(xSeg*2.0*M_PI)*sin(ySeg*M_PI);
            float yPos=cos(ySeg*M_PI);
            float zPos=sin(xSeg*2.0*M_PI)*sin(ySeg*M_PI);
            pos.push_back(glm::vec3(xPos,yPos,zPos));
            norm.push_back(glm::vec3(xPos,yPos,zPos));
        }
    }
    for(unsigned int y=0;y<Y_SEGMENTS;y++){
        for(unsigned int x=0;x<X_SEGMENTS;x++){
            unsigned int a=y*(X_SEGMENTS+1)+x;
            unsigned int b=(y+1)*(X_SEGMENTS+1)+x;
            unsigned int c=(y+1)*(X_SEGMENTS+1)+x+1;
            unsigned int d=y*(X_SEGMENTS+1)+x+1;
            idx.push_back(a); idx.push_back(b); idx.push_back(d);
            idx.push_back(b); idx.push_back(c); idx.push_back(d);
        }
    }

    indexCount = static_cast<unsigned int>(idx.size());
    std::vector<float> data; data.reserve(pos.size()*6);
    for(size_t i=0;i<pos.size();i++){
        data.push_back(pos[i].x); data.push_back(pos[i].y); data.push_back(pos[i].z);
        data.push_back(norm[i].x); data.push_back(norm[i].y); data.push_back(norm[i].z);
    }

    glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO); glGenBuffers(1,&EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),idx.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

inline unsigned int Sphere::createShader(const char* vs,const char* fs){
    unsigned int vertex=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vertex,1,&vs,NULL); glCompileShader(vertex);
    int success; char info[512]; glGetShaderiv(vertex,GL_COMPILE_STATUS,&success);
    if(!success){ glGetShaderInfoLog(vertex,512,NULL,info); std::cout<<"VS: "<<info<<"\n"; }
    unsigned int frag=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(frag,1,&fs,NULL); glCompileShader(frag);
    glGetShaderiv(frag,GL_COMPILE_STATUS,&success);
    if(!success){ glGetShaderInfoLog(frag,512,NULL,info); std::cout<<"FS: "<<info<<"\n"; }
    unsigned int program=glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,frag); glLinkProgram(program);
    glGetProgramiv(program,GL_LINK_STATUS,&success);
    if(!success){ glGetProgramInfoLog(program,512,NULL,info); std::cout<<"Link: "<<info<<"\n"; }
    glDeleteShader(vertex); glDeleteShader(frag);
    return program;
}

inline void Sphere::setMaterial(const glm::vec3& col, float a, float m, float r, float ambient, float refl, float trans, float indexOfRefraction){
    color=col; alpha=a; metallic=m; roughness=r; ao=ambient; reflectionStrength=refl;
    transmission = trans; ior = indexOfRefraction;
}

inline void Sphere::draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
                         const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir) const
{
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"lightSpaceMatrix"),1,GL_FALSE,glm::value_ptr(lightSpaceMatrix));

    glUniform3fv(glGetUniformLocation(shaderProgram,"cameraPos"),1,glm::value_ptr(cameraPos));
    glUniform3fv(glGetUniformLocation(shaderProgram,"materialColor"),1,glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shaderProgram,"alpha"),alpha);
    glUniform1f(glGetUniformLocation(shaderProgram,"metallic"),metallic);
    glUniform1f(glGetUniformLocation(shaderProgram,"roughness"),roughness);
    glUniform1f(glGetUniformLocation(shaderProgram,"ao"),ao);
    glUniform1f(glGetUniformLocation(shaderProgram,"reflectionStrength"),reflectionStrength);
    glUniform3fv(glGetUniformLocation(shaderProgram,"lightDir"),1,glm::value_ptr(lightDir));

    // Nové uniformy
    glUniform1f(glGetUniformLocation(shaderProgram, "transmission"), transmission);
    glUniform1f(glGetUniformLocation(shaderProgram, "ior"), ior);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP,envCubemap);
    glUniform1i(glGetUniformLocation(shaderProgram,"environmentMap"),0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,shadowMap);
    glUniform1i(glGetUniformLocation(shaderProgram,"shadowMap"),1);

    // Zapnutí a vypnutí blendování se teď řídí podle materiálu
    if(transmission > 0.0){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);

    if(transmission > 0.0){
        glDisable(GL_BLEND);
    }
}

inline void Sphere::drawForShadow(unsigned int depthShader,const glm::mat4& model,const glm::mat4& lightSpaceMatrix) const{
    glUseProgram(depthShader);
    glUniformMatrix4fv(glGetUniformLocation(depthShader,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(depthShader,"lightSpaceMatrix"),1,GL_FALSE,glm::value_ptr(lightSpaceMatrix));
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}

#endif
