#pragma once

#include "Transform.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifndef aiTextureType_BASE_COLOR
#define aiTextureType_BASE_COLOR aiTextureType_DIFFUSE
#endif

#ifndef aiTextureType_METALNESS
#define aiTextureType_METALNESS aiTextureType_UNKNOWN
#endif

#ifndef aiTextureType_DIFFUSE_ROUGHNESS
#define aiTextureType_DIFFUSE_ROUGHNESS aiTextureType_UNKNOWN
#endif

#ifndef aiTextureType_SHININESS
#define aiTextureType_SHININESS aiTextureType_SPECULAR
#endif

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <iostream>

static GLuint compileShader(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){
        GLint len=0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len,'\0'); glGetShaderInfoLog(s,len,nullptr,log.data());
        std::cerr << "Shader compile error (" << (type==GL_VERTEX_SHADER?"VS":"FS") << "):\n" << log << std::endl;
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs){
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){
        GLint len=0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len,'\0'); glGetProgramInfoLog(p,len,nullptr,log.data());
        std::cerr << "Program link error:\n" << log << std::endl;
    }
    glDetachShader(p,vs); glDetachShader(p,fs);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// shader (Blinn-Phong + normal map + metalness/smoothness)
static const char* kDefaultVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec2 vUV;
out mat3 vTBN;

void main(){
    vec4 worldPos = uModel * vec4(aPos,1.0);
    vWorldPos = worldPos.xyz;
    vUV = aUV;

    vec3 T = mat3(uModel) * aTangent;
    vec3 B = mat3(uModel) * aBitangent;
    vec3 N = mat3(uModel) * aNormal;
    vTBN = mat3(normalize(T), normalize(B), normalize(N));

    gl_Position = uProj * uView * worldPos;
}
)GLSL";

static const char* kDefaultFS = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec2 vUV;
in mat3 vTBN;

struct TexSet {
    sampler2D albedo;
    sampler2D normal;
    sampler2D metallic;
    sampler2D smoothness;
};

uniform TexSet uTex;

uniform bool uHasAlbedo;
uniform bool uHasNormal;
uniform bool uHasMetallic;
uniform bool uHasSmoothness;

uniform vec3 uAlbedoColor;
uniform float uMetallicFactor;
uniform float uSmoothnessFactor;

// --- Nové uniformy pro světlo ---
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uAmbientStrength;
uniform vec3 uCameraPos;

vec3 getNormal(){
    vec3 N = normalize(vTBN[2]);
    if(uHasNormal){
        vec3 n = texture(uTex.normal, vUV).xyz * 2.0 - 1.0;
        N = normalize(vTBN * n);
    }
    return N;
}

void main(){
    vec3 albedo = uHasAlbedo ? pow(texture(uTex.albedo, vUV).rgb, vec3(2.2)) : uAlbedoColor;
    float metallic = uHasMetallic ? texture(uTex.metallic, vUV).r : uMetallicFactor;
    float smoothness = uHasSmoothness ? texture(uTex.smoothness, vUV).r : uSmoothnessFactor;

    vec3 N = getNormal();

    // Směr ke světlu
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L+V);

    float NdotL = max(dot(N,L), 0.0);
    float NdotH = max(dot(N,H), 0.0);

    float shininess = mix(8.0, 128.0, smoothness);
    float spec = pow(NdotH, shininess);

    // Osvětlení
    vec3 diffuse = albedo * NdotL;
    vec3 specular = mix(vec3(0.04), albedo, metallic) * spec * NdotL;

    // Ambient složka (nastavitelná)
    vec3 ambient = albedo * uAmbientStrength;

    vec3 color = ambient + (diffuse + specular) * uLightColor;

    // Gamma korekce
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}

)GLSL";

struct Mesh {
    GLuint vao=0, vbo=0, ebo=0;
    GLsizei indexCount=0;
    GLuint texAlbedo=0;
    GLuint texNormal=0;
    GLuint texMetallic=0;
    GLuint texSmoothness=0;
};


class ModelFBX {

private:

    std::vector<Mesh> meshes_;
    std::string directory_;
    GLuint program_ = 0;
    float fallbackAlbedo_[3] = {0.8f, 0.8f, 0.85f};
    float fallbackMetallic_ = 0.0f;
    float fallbackSmoothness_ = 0.2f;
    std::vector<GLuint> ownedTextures_;
    std::unordered_map<std::string, GLuint> cacheTextures_;

public:

    ModelFBX(const std::string& path, const std::string& vsSrc = kDefaultVS,const std::string& fsSrc = kDefaultFS,bool flipUVs = false)
    {
        directory_ = std::filesystem::path(path).parent_path().string();
        loadModel(path, flipUVs);
        createProgram(vsSrc.c_str(), fsSrc.c_str());
    }

    ~ModelFBX(){
        for(auto& m : meshes_){
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ebo);
        }
        for(auto id : ownedTextures_){ glDeleteTextures(1, &id); }
        if(program_) glDeleteProgram(program_);
    }

    Transform transform;
    void setFallbackAlbedo(float r, float g, float b){ fallbackAlbedo_[0]=r; fallbackAlbedo_[1]=g; fallbackAlbedo_[2]=b; }
    void setFallbackMetallic(float v){ fallbackMetallic_ = v; }
    void setFallbackSmoothness(float v){ fallbackSmoothness_ = v; }

    void DrawForShadow(unsigned int depthShaderID, const glm::mat4& lightSpaceMatrix) const {
        glUseProgram(depthShaderID);
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "model"), 1, GL_FALSE, glm::value_ptr(transform.GetModelMatrix()));
        for(const auto& m : meshes_){
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

    }
    void setLightProperties(const glm::vec3& lightPos,
                            const glm::vec3& lightColor,
                            float ambientStrength,
                            const glm::vec3& cameraPos)
    {
        glUseProgram(program_);
        glUniform3fv(glGetUniformLocation(program_, "uLightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(program_, "uLightColor"), 1, glm::value_ptr(lightColor));
        glUniform1f(glGetUniformLocation(program_, "uAmbientStrength"), ambientStrength);
        glUniform3fv(glGetUniformLocation(program_, "uCameraPos"), 1, glm::value_ptr(cameraPos));
    }

    void draw(const glm::mat4& view,const glm::mat4& proj,const glm::vec3& cameraPos){

        glUseProgram(program_);
        glm::mat4 model = transform.GetModelMatrix();
        glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program_, "uView"),  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program_, "uProj"),  1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(program_, "uCameraPos"), 1, glm::value_ptr(cameraPos));

        glUniform1i(glGetUniformLocation(program_, "uTex.albedo"), 0);
        glUniform1i(glGetUniformLocation(program_, "uTex.normal"), 1);
        glUniform1i(glGetUniformLocation(program_, "uTex.metallic"), 2);
        glUniform1i(glGetUniformLocation(program_, "uTex.smoothness"), 3);

        glUniform3fv(glGetUniformLocation(program_, "uAlbedoColor"), 1, fallbackAlbedo_);
        glUniform1f(glGetUniformLocation(program_, "uMetallicFactor"), fallbackMetallic_);
        glUniform1f(glGetUniformLocation(program_, "uSmoothnessFactor"), fallbackSmoothness_);

        for(const auto& m : meshes_){
            bindTextureWithFallback(m.texAlbedo, 0, "uHasAlbedo");
            bindTextureWithFallback(m.texNormal, 1, "uHasNormal");
            bindTextureWithFallback(m.texMetallic, 2, "uHasMetallic");
            bindTextureWithFallback(m.texSmoothness, 3, "uHasSmoothness");

            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        glUseProgram(0);
    }

    GLuint program() const { return program_; }

private:


    void createProgram(const char* vs, const char* fs){
        GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        program_ = linkProgram(v, f);
    }

    void bindTextureWithFallback(GLuint tex, int unit, const char* hasName){
        glUniform1i(glGetUniformLocation(program_, hasName), tex!=0);
        if(tex){
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, tex);
        }
    }

    void loadModel(const std::string& path, bool flipUVs){
        Assimp::Importer importer;
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices;
        if(flipUVs) flags |= aiProcess_FlipUVs;
        const aiScene* scene = importer.ReadFile(path, flags);
        if(!scene || !scene->mRootNode){
            std::cerr << "Assimp: nepodarilo se nacist soubor: " << importer.GetErrorString() << std::endl;
            return;
        }
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene){
        for(unsigned i=0;i<node->mNumMeshes;++i){
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes_.push_back(processMesh(mesh, scene));
        }
        for(unsigned i=0;i<node->mNumChildren;++i){ processNode(node->mChildren[i], scene); }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene){

        std::vector<float> vertices; vertices.reserve(mesh->mNumVertices * 14);
        std::vector<unsigned int> indices; indices.reserve(mesh->mNumFaces * 3);

        for(unsigned i=0;i<mesh->mNumVertices;++i){
            aiVector3D p = mesh->mVertices[i];
            aiVector3D n = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0,1,0);
            aiVector3D t = mesh->mTangents ? mesh->mTangents[i] : aiVector3D(1,0,0);
            aiVector3D b = mesh->mBitangents ? mesh->mBitangents[i] : aiVector3D(0,0,1);
            aiVector3D uv = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);

            vertices.insert(vertices.end(), {p.x,p.y,p.z, n.x,n.y,n.z, uv.x,uv.y, t.x,t.y,t.z, b.x,b.y,b.z});
        }
        for(unsigned f=0; f<mesh->mNumFaces; ++f){
            const aiFace& face = mesh->mFaces[f];
            for(unsigned j=0;j<face.mNumIndices;++j) indices.push_back(face.mIndices[j]);
        }

        Mesh out;
        glGenVertexArrays(1, &out.vao);
        glGenBuffers(1, &out.vbo);
        glGenBuffers(1, &out.ebo);

        glBindVertexArray(out.vao);
        glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        GLsizei stride = (3+3+2+3+3)*sizeof(float);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,stride,(void*)(8*sizeof(float)));
        glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,stride,(void*)(11*sizeof(float)));

        glBindVertexArray(0);
        out.indexCount = static_cast<GLsizei>(indices.size());

        if(mesh->mMaterialIndex >= 0){
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiString name;
            mat->Get(AI_MATKEY_NAME, name);   // bezpečný způsob
            std::cout << "Mesh has material: "
                      << (name.length > 0 ? name.C_Str() : "<unnamed>")
                      << std::endl;

            for (int type = aiTextureType_DIFFUSE; type <= aiTextureType_UNKNOWN; ++type) {
                aiTextureType t = static_cast<aiTextureType>(type);
                for (unsigned int i = 0; i < mat->GetTextureCount(t); ++i) {
                    aiString texPath;
                    if (mat->GetTexture(t, i, &texPath) == AI_SUCCESS) {
                        std::cout << "  Texture [" << type << "] = " << texPath.C_Str() << std::endl;
                    }
                }
            }

            out.texAlbedo = loadFirstTexture(mat, {aiTextureType_DIFFUSE});
            out.texNormal = loadFirstTexture(mat, {aiTextureType_NORMALS, aiTextureType_HEIGHT});
            out.texMetallic = loadFirstTexture(mat, {aiTextureType_SPECULAR});
            out.texSmoothness = loadFirstTexture(mat, {aiTextureType_SHININESS});
        }
        if(mesh->HasTextureCoords(0)) {
            for(int i = 0; i < std::min<unsigned>(3, mesh->mNumVertices); i++) {
                auto uv = mesh->mTextureCoords[0][i];
                std::cout << "UV[" << i << "] = (" << uv.x << ", " << uv.y << ")\n";
            }
        } else {
            std::cout << "Mesh has NO UVs\n";
        }
        std::cout << "Mesh has " << mesh->GetNumUVChannels() << " UV channels\n";
        return out;
    }

    GLuint loadFirstTexture(aiMaterial* mat, std::initializer_list<aiTextureType> types){
        for(auto type : types){
            if(mat->GetTextureCount(type) > 0){
                aiString str;
                mat->GetTexture(type, 0, &str);
                std::string p = str.C_Str();
                std::string resolved = resolvePath(p);

                std::cout << "Texture found: " << p
                          << " -> resolved path: " << resolved << std::endl;

                return loadTexture2D(resolved);
            }
        }
        return 0;
    }

    std::string resolvePath(const std::string& p){
        std::filesystem::path path(p);
        if(path.is_absolute()) return path.string();

        // 1) hledání přímo vedle FBX
        auto joined = std::filesystem::path(directory_) / path;
        if(std::filesystem::exists(joined)) return joined.string();

        // 2) hledání podle jména ve složce FBX
        auto fname = std::filesystem::path(p).filename();
        joined = std::filesystem::path(directory_) / fname;
        if(std::filesystem::exists(joined)) return joined.string();

        // 3) hledání v podadresáři Textures/
        joined = std::filesystem::path(directory_) / "Textures" / fname;
        if(std::filesystem::exists(joined)) return joined.string();

        std::cerr << "Warning: textura nenalezena: " << p << std::endl;
        std::cout << "Texture found: " << p
                  << " -> resolved path: " << fname.string() << std::endl;
        return fname.string(); // fallback
    }



    GLuint loadTexture2D(const std::string& file){
        auto it = cacheTextures_.find(file);
        if(it != cacheTextures_.end()) return it->second;
        stbi_set_flip_vertically_on_load(true);
        int w,h,n; stbi_uc* data = stbi_load(file.c_str(), &w,&h,&n, 0);
        if(!data){
            std::cerr << "stb_image: nelze nacist texturu: " << file << std::endl;
            return 0;
        }
        GLenum format = GL_RGB;
        if(n==1) format = GL_RED; else if(n==3) format = GL_RGB; else if(n==4) format = GL_RGBA;

        GLuint tex=0; glGenTextures(1, &tex); ownedTextures_.push_back(tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, n==4?GL_SRGB_ALPHA:GL_SRGB, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);

        cacheTextures_[file] = tex;
        return tex;
    }
};


