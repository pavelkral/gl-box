#ifndef SPHERE_H
#define SPHERE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <iostream>


const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

out vec3 WorldPos;
out vec3 Normal;
out vec2 UV;
out mat3 TBN;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    UV = aUV;

    // Vypočet TBN matice pro normal mapping
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt proces pro ortogonalizaci
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    Normal = N; // Použijeme transformovanou normálu
    FragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)glsl";

const char* fragmentShaderSrc = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 UV;
in mat3 TBN;
in vec4 FragPosLightSpace;

// Uniforms pro material a sceny
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform samplerCube environmentMap;
uniform sampler2D shadowMap;

// Uniforms pro PBR parametry (pokud nejsou textury)
uniform vec3 materialColor;
uniform float alpha;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform float reflectionStrength;
uniform float transmission;
uniform float ior;

// Uniforms pro textury
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

// Bool prepinace pro pouziti textur
uniform bool useAlbedoMap;
uniform bool useNormalMap;
uniform bool useMetallicMap;
uniform bool useRoughnessMap;
uniform bool useAoMap;


const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 5.0;

// --- Fresnel, Geometry, Distribution ---
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// --- Shadow mapping ---
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

// --- ACES Filmic tonemapper ---
vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFilm(vec3 color)
{
    color *= 1.0;
    color = RRTAndODTFit(color);
    return pow(color, vec3(1.0/2.2));
}

void main()
{
    // Ziskani PBR hodnot z textur nebo z uniformu
    vec3 albedo     = useAlbedoMap   ? pow(texture(albedoMap, UV).rgb, vec3(2.2)) : pow(materialColor, vec3(2.2));
    float metallicVal = useMetallicMap ? texture(metallicMap, UV).r                : metallic;
    float roughnessVal= useRoughnessMap? texture(roughnessMap, UV).r               : roughness;
    float aoVal       = useAoMap       ? texture(aoMap, UV).r                      : ao;

    // Vypocet normaly (z normal mapy nebo z vertex dat)
    vec3 N = normalize(Normal);
    if(useNormalMap) {
        vec3 tangentNormal = texture(normalMap, UV).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(cameraPos - WorldPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    // --- Automatic F0 assignment ---
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallicVal);

    // --- Transparent materials ---
    if (transmission > 0.0)
    {
        float ratio = 1.0 / ior;
        vec3 T = refract(-V, N, ratio);
        vec3 refractedColor = textureLod(environmentMap, T, roughnessVal * MAX_REFLECTION_LOD).rgb;
        vec3 R = reflect(-V, N);
        vec3 reflectedColor = textureLod(environmentMap, R, roughnessVal * MAX_REFLECTION_LOD).rgb;
        vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
        vec3 color = mix(refractedColor, reflectedColor, F);
        FragColor = vec4(ACESFilm(color), alpha);
        return;
    }

    // --- Lighting ---
    float NDF = DistributionGGX(N, H, roughnessVal);
    float G   = GeometrySmith(N, V, L, roughnessVal);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallicVal);

    vec3 diffuse = albedo; // Diffuse is now just the albedo
    vec3 specular = (NDF * G * F) / max(4.0 * max(dot(N, V),0.0)*max(dot(N,L),0.0),0.0001);

    float shadow = ShadowCalculation(FragPosLightSpace, N, L);

    // Multiply direct light by light color
    vec3 directLight = (kD * diffuse / PI + specular) * max(dot(N,L),0.0) * (1.0 - shadow) * lightColor;

    // --- Image-based lighting (IBL) ---
    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = textureLod(environmentMap, R, roughnessVal * MAX_REFLECTION_LOD).rgb;
    vec3 F_env = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 ambient = (kD * diffuse + F_env * prefilteredColor) * aoVal * reflectionStrength;

    vec3 color = directLight + ambient;
    FragColor = vec4(ACESFilm(color), alpha);
}
)glsl";


// =================================================================================================
// C++ TŘÍDA
// =================================================================================================

class Sphere
{
private:
    unsigned int shaderProgram;
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;

    void initGeometry();
    static unsigned int createShader(const char* vs, const char* fs);

public:
    // PBR parametry
    glm::vec3 color;
    float alpha;
    float metallic;
    float roughness;
    float ao;
    float reflectionStrength;
    float transmission;
    float ior;

    // ID textur
    unsigned int albedoMapID;
    unsigned int normalMapID;
    unsigned int metallicMapID;
    unsigned int roughnessMapID;
    unsigned int aoMapID;

    Sphere();
    ~Sphere();

    // Metody pro nastavení
    void setMaterial(const glm::vec3& col, float a, float m, float r, float ambient, float refl = 1.0f, float trans = 0.0f, float indexOfRefraction = 1.52f);
    void setAlbedoTexture(unsigned int texID) { albedoMapID = texID; }
    void setNormalTexture(unsigned int texID) { normalMapID = texID; }
    void setMetallicTexture(unsigned int texID) { metallicMapID = texID; }
    void setRoughnessTexture(unsigned int texID) { roughnessMapID = texID; }
    void setAoTexture(unsigned int texID) { aoMapID = texID; }

    void draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
              const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir,const glm::vec3& lightCol) const;

    void drawForShadow(unsigned int depthShader, const glm::mat4& model, const glm::mat4& lightSpaceMatrix) const;
};


inline Sphere::Sphere(){
    shaderProgram = createShader(vertexShaderSrc,fragmentShaderSrc);
    initGeometry();
    color = glm::vec3(1.0f);
    alpha = 1.0f;
    metallic = 0.0f;
    roughness = 0.5f;
    ao = 1.0f;
    reflectionStrength = 1.0f;
    transmission = 0.0f;
    ior = 1.52f;
    albedoMapID = 0;
    normalMapID = 0;
    metallicMapID = 0;
    roughnessMapID = 0;
    aoMapID = 0;
}

inline Sphere::~Sphere(){
    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteBuffers(1,&EBO);
    glDeleteProgram(shaderProgram);
}

inline void Sphere::initGeometry(){
    const float M_PI = 3.14159265359f;
    const unsigned int X_SEGMENTS = 64, Y_SEGMENTS = 64;
    std::vector<glm::vec3> pos, norm, tan;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> idx;

    for(unsigned int y=0;y<=Y_SEGMENTS;y++){
        for(unsigned int x=0;x<=X_SEGMENTS;x++){
            float xSeg=(float)x/X_SEGMENTS, ySeg=(float)y/Y_SEGMENTS;
            float xPos=cos(xSeg*2.0*M_PI)*sin(ySeg*M_PI);
            float yPos=cos(ySeg*M_PI);
            float zPos=sin(xSeg*2.0*M_PI)*sin(ySeg*M_PI);
            pos.push_back(glm::vec3(xPos,yPos,zPos));
            norm.push_back(glm::vec3(xPos,yPos,zPos));
            uvs.push_back(glm::vec2(xSeg, ySeg));
            // Výpočet tangenty pro normal mapping
            glm::vec3 tangent = glm::normalize(glm::vec3(-sin(xSeg * 2.0f * M_PI), 0, cos(xSeg * 2.0f * M_PI)));
            tan.push_back(tangent);
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
    std::vector<float> data;
    // Vertex data: position(3), normal(3), uv(2), tangent(3) -> 11 floats per vertex
    data.reserve(pos.size()*11);
    for(size_t i=0;i<pos.size();i++){
        data.push_back(pos[i].x); data.push_back(pos[i].y); data.push_back(pos[i].z);
        data.push_back(norm[i].x);data.push_back(norm[i].y);data.push_back(norm[i].z);
        data.push_back(uvs[i].x); data.push_back(uvs[i].y);
        data.push_back(tan[i].x); data.push_back(tan[i].y); data.push_back(tan[i].z);
    }

    glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO); glGenBuffers(1,&EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),idx.data(),GL_STATIC_DRAW);

    // Stride je nyní 11 floats
    GLsizei stride = 11 * sizeof(float);
    // Position
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    // Normal
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    // UV
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    // Tangent
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,stride,(void*)(8*sizeof(float)));

    glBindVertexArray(0);
}

inline unsigned int Sphere::createShader(const char* vs,const char* fs){
    unsigned int vertex=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex,1,&vs,NULL); glCompileShader(vertex);
    int success; char info[512]; glGetShaderiv(vertex,GL_COMPILE_STATUS,&success);
    if(!success){ glGetShaderInfoLog(vertex,512,NULL,info); std::cout<<"VS: "<<info<<"\n"; }
    unsigned int frag=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag,1,&fs,NULL); glCompileShader(frag);
    glGetShaderiv(frag,GL_COMPILE_STATUS,&success);
    if(!success){ glGetShaderInfoLog(frag,512,NULL,info); std::cout<<"FS: "<<info<<"\n"; }
    unsigned int program=glCreateProgram();
    glAttachShader(program,vertex); glAttachShader(program,frag); glLinkProgram(program);
    glGetProgramiv(program,GL_LINK_STATUS,&success);
    if(!success){ glGetProgramInfoLog(program,512,NULL,info); std::cout<<"Link: "<<info<<"\n"; }
    glDeleteShader(vertex); glDeleteShader(frag);
    return program;
}

inline void Sphere::setMaterial(const glm::vec3& col, float a, float m, float r,
                                float ambient, float refl, float trans, float indexOfRefraction){
    color=col; alpha=a; metallic=m; roughness=r; ao=ambient; reflectionStrength=refl;
    transmission = trans; ior = indexOfRefraction;
}

inline void Sphere::draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
                         const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir,const glm::vec3& lightCol) const
{
    glUseProgram(shaderProgram);

    // Nastavení matic a základních parametrů (beze změny)
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"lightSpaceMatrix"),1,GL_FALSE,glm::value_ptr(lightSpaceMatrix));
    glUniform3fv(glGetUniformLocation(shaderProgram,"cameraPos"),1,glm::value_ptr(cameraPos));
    glUniform3fv(glGetUniformLocation(shaderProgram,"lightDir"),1,glm::value_ptr(lightDir));
    glUniform3fv(glGetUniformLocation(shaderProgram,"lightColor"), 1, glm::value_ptr(lightCol));

    // Nastavení PBR parametrů (beze změny)
    glUniform3fv(glGetUniformLocation(shaderProgram,"materialColor"),1,glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shaderProgram,"alpha"),alpha);
    glUniform1f(glGetUniformLocation(shaderProgram,"metallic"),metallic);
    glUniform1f(glGetUniformLocation(shaderProgram,"roughness"),roughness);
    glUniform1f(glGetUniformLocation(shaderProgram,"ao"),ao);
    glUniform1f(glGetUniformLocation(shaderProgram,"reflectionStrength"),reflectionStrength);
    glUniform1f(glGetUniformLocation(shaderProgram, "transmission"), transmission);
    glUniform1f(glGetUniformLocation(shaderProgram, "ior"), ior);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUniform1i(glGetUniformLocation(shaderProgram,"environmentMap"),0);

    // Jednotka 1: Shadow Map (sampler2D)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    // Pojistka: Ujistíme se, že na této jednotce není navázaná žádná cubemapa
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glUniform1i(glGetUniformLocation(shaderProgram,"shadowMap"),1);

    // Pro ostatní textury je to méně kritické, protože jsou všechny typu 2D,
    // ale je to dobrý zvyk.

    // Jednotka 2: Albedo Map
    bool useAlbedo = (albedoMapID != 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useAlbedoMap"), useAlbedo);
    if(useAlbedo) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, albedoMapID);
        glUniform1i(glGetUniformLocation(shaderProgram, "albedoMap"), 2);
    }

    // Jednotka 3: Normal Map
    bool useNormal = (normalMapID != 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useNormalMap"), useNormal);
    if(useNormal) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, normalMapID);
        glUniform1i(glGetUniformLocation(shaderProgram, "normalMap"), 3);
    }

    // Jednotka 4: Metallic Map
    bool useMetallic = (metallicMapID != 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useMetallicMap"), useMetallic);
    if(useMetallic) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, metallicMapID);
        glUniform1i(glGetUniformLocation(shaderProgram, "metallicMap"), 4);
    }

    // Jednotka 5: Roughness Map
    bool useRoughness = (roughnessMapID != 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useRoughnessMap"), useRoughness);
    if(useRoughness) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, roughnessMapID);
        glUniform1i(glGetUniformLocation(shaderProgram, "roughnessMap"), 5);
    }

    // Jednotka 6: AO Map
    bool useAo = (aoMapID != 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useAoMap"), useAo);
    if(useAo) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, aoMapID);
        glUniform1i(glGetUniformLocation(shaderProgram, "aoMap"), 6);
    }


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
