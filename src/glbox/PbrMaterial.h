#ifndef PBRMATERIAL_H
#define PBRMATERIAL_H

#include "Shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

const char* pbrVertexShaderSrc = R"glsl(
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

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    Normal = N;
    FragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)glsl";

const char* pbrFragmentShaderSrc = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 UV;
in mat3 TBN;
in vec4 FragPosLightSpace;

// Uniforms
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform samplerCube environmentMap;
uniform sampler2D shadowMap;

// PBR material properties
uniform vec3 materialColor;
uniform float alpha;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform float reflectionStrength;
uniform float transmission;
uniform float ior;

// Textures
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

// Texture usage flags
uniform bool useAlbedoMap;
uniform bool useNormalMap;
uniform bool useMetallicMap;
uniform bool useRoughnessMap;
uniform bool useAoMap;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 5.0;

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

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFilm(vec3 color) {
    color = RRTAndODTFit(color);
    return pow(color, vec3(1.0/2.2));
}

void main()
{
    vec3 albedo       = useAlbedoMap    ? pow(texture(albedoMap, UV).rgb, vec3(2.2)) : pow(materialColor, vec3(2.2));
    float metallicVal = useMetallicMap  ? texture(metallicMap, UV).r                : metallic;
    float roughnessVal= useRoughnessMap ? texture(roughnessMap, UV).r               : roughness;
    float aoVal       = useAoMap        ? texture(aoMap, UV).r                      : ao;

    vec3 N = normalize(Normal);
    if(useNormalMap) {
        vec3 tangentNormal = texture(normalMap, UV).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(cameraPos - WorldPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallicVal);

    if (transmission > 0.0) {
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

    float NDF = DistributionGGX(N, H, roughnessVal);
    float G   = GeometrySmith(N, V, L, roughnessVal);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallicVal);

    vec3 diffuse = albedo;
    vec3 specular = (NDF * G * F) / max(4.0 * max(dot(N, V),0.0)*max(dot(N,L),0.0),0.0001);

    float shadow = ShadowCalculation(FragPosLightSpace, N, L);
    vec3 directLight = (kD * diffuse / PI + specular) * max(dot(N,L),0.0) * (1.0 - shadow) * lightColor;

    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = textureLod(environmentMap, R, roughnessVal * MAX_REFLECTION_LOD).rgb;
    vec3 F_env = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 ambient = (kD * diffuse + F_env * prefilteredColor) * aoVal * reflectionStrength;

    vec3 color = directLight + ambient;
    FragColor = vec4(ACESFilm(color), alpha);
}
)glsl";


class PbrMaterial {
public:
    unsigned int shaderProgramID;

    glm::vec3 albedoColor;
    float alpha;
    float metallic;
    float roughness;
    float ao;
    float reflectionStrength;
    float transmission;
    float ior;

    // ID texturE (0 = NO texturE)
    unsigned int albedoMapID;
    unsigned int normalMapID;
    unsigned int metallicMapID;
    unsigned int roughnessMapID;
    unsigned int aoMapID;

    PbrMaterial() {
       // shaderProgramID = createShader(pbrVertexShaderSrc, pbrFragmentShaderSrc);
        Shader shaderProgram(pbrVertexShaderSrc, pbrFragmentShaderSrc,true);
        shaderProgramID = shaderProgram.ID;
        albedoColor = glm::vec3(0.8f); alpha = 1.0f; metallic = 0.0f;
        roughness = 0.5f; ao = 1.0f; reflectionStrength = 1.0f;
        transmission = 0.0f; ior = 1.52f;
        albedoMapID = 0; normalMapID = 0; metallicMapID = 0;
        roughnessMapID = 0; aoMapID = 0;
    }

    ~PbrMaterial() {
        glDeleteProgram(shaderProgramID);
    }


    void setAlbedoMap(unsigned int texID)   { albedoMapID = texID; }
    void setNormalMap(unsigned int texID)   { normalMapID = texID; }
    void setRoughnessMap(unsigned int texID){ roughnessMapID = texID; }
    void setMetallicMap(unsigned int texID) { metallicMapID = texID; }
    void setAoMap(unsigned int texID)       { aoMapID = texID; }
    // ------------------------------------

    void use(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
             const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
             const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir, const glm::vec3& lightCol) const {

        glUseProgram(shaderProgramID);

        setMat4("model", model); setMat4("view", view); setMat4("projection", proj);
        setMat4("lightSpaceMatrix", lightSpaceMatrix); setVec3("cameraPos", cameraPos);
        setVec3("lightDir", lightDir); setVec3("lightColor", lightCol);


        setVec3("materialColor", albedoColor); setFloat("alpha", alpha);
        setFloat("metallic", metallic); setFloat("roughness", roughness);
        setFloat("ao", ao); setFloat("reflectionStrength", reflectionStrength);
        setFloat("transmission", transmission); setFloat("ior", ior);

        // Vazba textur: 0=Env, 1=Shadow, 2+=Material maps
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap); setInt("environmentMap", 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadowMap); setInt("shadowMap", 1);

        bindTexture(2, "albedoMap",   "useAlbedoMap",   albedoMapID);
        bindTexture(3, "normalMap",   "useNormalMap",   normalMapID);
        bindTexture(4, "metallicMap", "useMetallicMap", metallicMapID);
        bindTexture(5, "roughnessMap","useRoughnessMap",roughnessMapID);
        bindTexture(6, "aoMap",       "useAoMap",       aoMapID);

        if (transmission > 0.0 || alpha < 1.0) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    void unuse() const {
        if (transmission > 0.0 || alpha < 1.0) {
            glDisable(GL_BLEND);
        }
    }
    void setParameters(
        const glm::vec3& albedoColor = glm::vec3(0.8f),
        float alpha = 1.0f,
        float metallic = 1.0f,
        float roughness = 1.0f,
        float ao = 1.0f,
        float reflectionStrength = 1.0f,
        float transmission = 1.0f,
        float ior = 1.0f)
    {
        this->albedoColor = albedoColor;
        this->alpha = alpha;
        this->metallic = metallic;
        this->roughness = roughness;
        this->ao = ao;
        this->reflectionStrength = reflectionStrength;
        this->transmission = transmission;
        this->ior = ior;
    }
private:
    void bindTexture(int unit, const std::string& samplerName, const std::string& useFlagName, unsigned int texID) const {
        bool useTexture = (texID != 0);
        setInt(useFlagName, useTexture);
        if (useTexture) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texID);
            setInt(samplerName, unit);
        }
    }

    void setMat4(const std::string& name, const glm::mat4& mat) const { glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat)); }
    void setVec3(const std::string& name, const glm::vec3& vec) const { glUniform3fv(glGetUniformLocation(shaderProgramID, name.c_str()), 1, glm::value_ptr(vec)); }
    void setInt(const std::string& name, int value) const { glUniform1i(glGetUniformLocation(shaderProgramID, name.c_str()), value); }
    void setFloat(const std::string& name, float value) const { glUniform1f(glGetUniformLocation(shaderProgramID, name.c_str()), value); }
};

#endif // PBRMATERIAL_H
