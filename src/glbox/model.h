#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

 #define GLM_ENABLE_EXPERIMENTAL

//#include <glm/gtx/component_wise.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "Transform.h"

// ---------- shaders (main skinning VS + lighting FS) ----------
static const char* kDefaultVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;
layout(location=5) in ivec4 aBoneIDs;
layout(location=6) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uBones[100];

out vec3 vWorldPos;
out vec2 vUV;
out mat3 vTBN;

void main(){
    // skinning: compute skin matrix (blend of bone transforms)
    mat4 skinMat = mat4(0.0);
    skinMat += aWeights.x * uBones[aBoneIDs.x];
    skinMat += aWeights.y * uBones[aBoneIDs.y];
    skinMat += aWeights.z * uBones[aBoneIDs.z];
    skinMat += aWeights.w * uBones[aBoneIDs.w];

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNormal = mat3(skinMat) * aNormal;
    vec3 skinnedTangent = mat3(skinMat) * aTangent;
    vec3 skinnedBitangent = mat3(skinMat) * aBitangent;

    vec4 worldPos = uModel * skinnedPos;
    vWorldPos = worldPos.xyz;
    vUV = aUV;

    vec3 T = normalize(mat3(uModel) * skinnedTangent);
    vec3 B = normalize(mat3(uModel) * skinnedBitangent);
    vec3 N = normalize(mat3(uModel) * skinnedNormal);
    vTBN = mat3(T, B, N);

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

    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L+V);

    float NdotL = max(dot(N,L), 0.0);
    float NdotH = max(dot(N,H), 0.0);

    float shininess = mix(8.0, 128.0, smoothness);
    float spec = pow(NdotH, shininess);

    vec3 diffuse = albedo * NdotL;
    vec3 specular = mix(vec3(0.04), albedo, metallic) * spec * NdotL;
    vec3 ambient = albedo * uAmbientStrength;

    vec3 color = ambient + (diffuse + specular) * uLightColor;
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}
)GLSL";

// ---------- depth shader for shadow map (skinning) ----------
static const char* kDepthVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=5) in ivec4 aBoneIDs;
layout(location=6) in vec4 aWeights;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;
uniform mat4 uBones[100];

void main() {
    mat4 skinMat = mat4(0.0);
    skinMat += aWeights.x * uBones[aBoneIDs.x];
    skinMat += aWeights.y * uBones[aBoneIDs.y];
    skinMat += aWeights.z * uBones[aBoneIDs.z];
    skinMat += aWeights.w * uBones[aBoneIDs.w];

    vec4 skinnedPos = skinMat * vec4(aPos,1.0);
    gl_Position = lightSpaceMatrix * model * skinnedPos;
}
)GLSL";

static const char* kDepthFS = R"GLSL(
#version 330 core
void main(){}
)GLSL";


static const int MAX_BONES = 100;

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

// ---------- data structures ----------

struct VertexBoneData {
    int ids[4];
    float weights[4];
    VertexBoneData(){ for(int i=0;i<4;i++){ ids[i]=0; weights[i]=0.0f; } }
    void addBoneData(int boneID, float weight) {
        for(int i=0;i<4;i++){
            if(weights[i] == 0.0f) { ids[i]=boneID; weights[i]=weight; return; }
        }
        // replace smallest if necessary
        int minI = 0; float minW = weights[0];
        for(int i=1;i<4;i++){ if(weights[i]<minW){ minW=weights[i]; minI=i; } }
        if(weight>minW){ ids[minI]=boneID; weights[minI]=weight; }
    }
};

struct BoneInfo {
    glm::mat4 offset;
    glm::mat4 finalTransform;
    BoneInfo(){ offset = glm::mat4(1.0f); finalTransform = glm::mat4(1.0f); }
};

struct Mesh {
    GLuint vao=0, vbo=0, ebo=0;
    GLuint boneVBO = 0;
    GLsizei indexCount=0;

    GLuint texAlbedo=0;
    GLuint texNormal=0;
    GLuint texMetallic=0;
    GLuint texSmoothness=0;
};

///============================================================================================

class ModelFBX {

private:
    std::vector<Mesh> meshes_;
    std::string directory_;
    GLuint program_ = 0;
    GLuint depthProgram_ = 0;
    float fallbackAlbedo_[3] = {0.8f, 0.8f, 0.85f};
    float fallbackMetallic_ = 0.0f;
    float fallbackSmoothness_ = 0.2f;
    std::vector<GLuint> ownedTextures_;
    std::unordered_map<std::string, GLuint> cacheTextures_;

    // animation
    Assimp::Importer importer_;
    const aiScene* scene_ = nullptr;
    std::unordered_map<std::string,int> boneMapping_; // name -> index
    std::vector<BoneInfo> bones_;
    int currentAnimIndex_ = 0;
    bool animPlaying_ = true;

    float loopStartTicks_ = 0.0f; // Počáteční čas v "ticích"
    float loopEndTicks_ = 0.0f;   // Koncový čas v "ticích"
    bool loopRangeActive_ = false; // Zda se má použít rozsah

public:
    ModelFBX(const std::string& path, const std::string& vsSrc = kDefaultVS,const std::string& fsSrc = kDefaultFS,bool flipUVs = false)
    {
        directory_ = std::filesystem::path(path).parent_path().string();
        loadModel(path, flipUVs);
        createProgram(vsSrc.c_str(), fsSrc.c_str());
        createDepthProgram(kDepthVS, kDepthFS);
    }

    ~ModelFBX(){
        for(auto& m : meshes_){
            if(m.vao) glDeleteVertexArrays(1, &m.vao);
            if(m.vbo) glDeleteBuffers(1, &m.vbo);
            if(m.ebo) glDeleteBuffers(1, &m.ebo);
            if(m.boneVBO) glDeleteBuffers(1, &m.boneVBO);
        }
        for(auto id : ownedTextures_){ glDeleteTextures(1, &id); }
        if(program_) glDeleteProgram(program_);
        if(depthProgram_) glDeleteProgram(depthProgram_);
    }

    Transform transform;
    void setFallbackAlbedo(float r, float g, float b){ fallbackAlbedo_[0]=r; fallbackAlbedo_[1]=g; fallbackAlbedo_[2]=b; }
    void setFallbackMetallic(float v){ fallbackMetallic_ = v; }
    void setFallbackSmoothness(float v){ fallbackSmoothness_ = v; }

    void setAlbedoTexture(GLuint textureID, size_t meshIndex) {
        if (meshIndex < meshes_.size()) {
            meshes_[meshIndex].texAlbedo = textureID;
        } else {
            std::cerr << "setAlbedoTexture: Neplatný index meshe: " << meshIndex << "\n";
        }
    }

    void setNormalTexture(GLuint textureID, size_t meshIndex) {
        if (meshIndex < meshes_.size()) {
            meshes_[meshIndex].texNormal = textureID;
        } else {
            std::cerr << "setNormalTexture: Neplatný index meshe: " << meshIndex << "\n";
        }
    }

    void setMetallicTexture(GLuint textureID, size_t meshIndex) {
        if (meshIndex < meshes_.size()) {
            meshes_[meshIndex].texMetallic = textureID;
        } else {
            std::cerr << "setMetallicTexture: Neplatný index meshe: " << meshIndex << "\n";
        }
    }

    void setSmoothnessTexture(GLuint textureID, size_t meshIndex) {
        if (meshIndex < meshes_.size()) {
            meshes_[meshIndex].texSmoothness = textureID;
        } else {
            std::cerr << "setSmoothnessTexture: Neplatný index meshe: " << meshIndex << "\n";
        }
    }

    size_t numMeshes() const {
        return meshes_.size();
    }

    /** Vrací ID Albedo textury pro daný mesh. */
    GLuint getAlbedoTexture(size_t meshIndex) const {
        if (meshIndex < meshes_.size()) {
            return meshes_[meshIndex].texAlbedo;
        }
        return 0;
    }


    void prepareBonesFallback() {
        if(bones_.empty()) {
            // Přidáme jednu "kost" s identitou
            BoneInfo bi;
            bi.finalTransform = glm::mat4(1.0f);
            bones_.push_back(bi);
        }
    }

    VertexBoneData createDefaultBoneData() {
        VertexBoneData vbd;
        vbd.ids[0] = 0;
        vbd.weights[0] = 1.0f;
        return vbd;
    }
    // draw (main shader)
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
        prepareBonesFallback();
        // upload bones
        size_t nBonesToSend = std::min<size_t>(bones_.size(), MAX_BONES);
        for(size_t i=0;i<nBonesToSend;i++){
            std::string name = "uBones[" + std::to_string(i) + "]";
            glUniformMatrix4fv(glGetUniformLocation(program_, name.c_str()), 1, GL_FALSE, glm::value_ptr(bones_[i].finalTransform));
        }

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

    // draw for shadow map (uses depthProgram_, supports skinning)
    void DrawForShadow(unsigned int depthShaderID, const glm::mat4& lightSpaceMatrix)  {
        // either user-supplied depthShaderID or internal depthProgram_ could be used.
        GLuint programToUse = depthShaderID ? depthShaderID : depthProgram_;
        glUseProgram(programToUse);

        // keep compatibility with original uniform names: "lightSpaceMatrix" and "model"
        glUniformMatrix4fv(glGetUniformLocation(programToUse, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glm::mat4 model = transform.GetModelMatrix();
        glUniformMatrix4fv(glGetUniformLocation(programToUse, "model"), 1, GL_FALSE, glm::value_ptr(model));
        prepareBonesFallback();
        // upload bones same as main draw
        size_t nBonesToSend = std::min<size_t>(bones_.size(), MAX_BONES);
        for(size_t i=0;i<nBonesToSend;i++){
            std::string name = "uBones[" + std::to_string(i) + "]";
            glUniformMatrix4fv(glGetUniformLocation(programToUse, name.c_str()), 1, GL_FALSE, glm::value_ptr(bones_[i].finalTransform));
        }

        for(const auto& m : meshes_){
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        glUseProgram(0);
    }

    GLuint program() const { return program_; }
    GLuint depthProgram() const { return depthProgram_; }
    size_t numBones() const { return bones_.size(); }
    glm::mat4 boneMatrix(size_t i) const { return (i < bones_.size()) ? bones_[i].finalTransform : glm::mat4(1.0f); }

private:
    // ---------- helpers ----------
    void createProgram(const char* vs, const char* fs){
        GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        program_ = linkProgram(v, f);
    }
    void createDepthProgram(const char* vs, const char* fs){
        GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        depthProgram_ = linkProgram(v, f);
    }

    void bindTextureWithFallback(GLuint tex, int unit, const char* hasName) const {
        glUniform1i(glGetUniformLocation(program_, hasName), tex!=0);
        if(tex){
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, tex);
        }
    }

    void loadModel(const std::string& path, bool flipUVs){
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices;
        if(flipUVs) flags |= aiProcess_FlipUVs;
        scene_ = importer_.ReadFile(path, flags);
        if(!scene_ || !scene_->mRootNode){
            std::cerr << "Assimp: nepodarilo se nacist soubor: " << importer_.GetErrorString() << std::endl;
            return;
        }
        // animation meta
        if(scene_->mNumAnimations > 0){
            const aiAnimation* anim = scene_->mAnimations[0];
            // not strictly needed to store here, keep for info
        }
        processNode(scene_->mRootNode, scene_);
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
        std::vector<VertexBoneData> boneData(mesh->mNumVertices);

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

        // load bones for this mesh
        if(mesh->HasBones()){
            for(unsigned int i=0;i<mesh->mNumBones;i++){
                std::string boneName(mesh->mBones[i]->mName.C_Str());
                int boneIndex = 0;
                if(boneMapping_.find(boneName) == boneMapping_.end()){
                    boneIndex = (int)bones_.size();
                    boneMapping_[boneName] = boneIndex;
                    BoneInfo bi;
                    bi.offset = aiMatToGlm(mesh->mBones[i]->mOffsetMatrix);
                    bones_.push_back(bi);
                } else {
                    boneIndex = boneMapping_[boneName];
                }
                for(unsigned int j=0; j<mesh->mBones[i]->mNumWeights; ++j){
                    unsigned int v = mesh->mBones[i]->mWeights[j].mVertexId;
                    float w = mesh->mBones[i]->mWeights[j].mWeight;
                    if(v < boneData.size())
                        boneData[v].addBoneData(boneIndex, w);
                }
            }
        } else {
            for(size_t i=0;i<mesh->mNumVertices;i++){
                boneData[i] = createDefaultBoneData();
            }
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

        glGenBuffers(1, &out.boneVBO);
        glBindBuffer(GL_ARRAY_BUFFER, out.boneVBO);
        glBufferData(GL_ARRAY_BUFFER, boneData.size()*sizeof(VertexBoneData), boneData.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(VertexBoneData), (void*)0);

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(VertexBoneData), (void*)(offsetof(VertexBoneData, weights)));

        glBindVertexArray(0);
        out.indexCount = static_cast<GLsizei>(indices.size());

        if(mesh->mMaterialIndex >= 0){
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiString name;
            mat->Get(AI_MATKEY_NAME, name);   // bezpečný způsob
            std::cout << "Mesh has material: " << (name.length > 0 ? name.C_Str() : "<unnamed>")<< std::endl;

            for (int type = aiTextureType_DIFFUSE; type <= aiTextureType_UNKNOWN; ++type) {
                aiTextureType t = static_cast<aiTextureType>(type);
                for (unsigned int i = 0; i < mat->GetTextureCount(t); ++i) {
                    aiString texPath;
                    if (mat->GetTexture(t, i, &texPath) == AI_SUCCESS) {
                        std::cout << "  Texture [" << type << "] = " << texPath.C_Str() << std::endl;
                    }
                }
            }

            out.texAlbedo = loadFirstTexture(mat, {aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR});
            out.texNormal = loadFirstTexture(mat, {aiTextureType_NORMALS, aiTextureType_HEIGHT});
            out.texMetallic = loadFirstTexture(mat, {aiTextureType_SPECULAR, aiTextureType_METALNESS});
            out.texSmoothness = loadFirstTexture(mat, {aiTextureType_SHININESS, aiTextureType_DIFFUSE_ROUGHNESS});
        }

        if(mesh->HasTextureCoords(0)) {
            for(int i = 0; i < std::min<unsigned>(3, mesh->mNumVertices); i++) {
                auto uv = mesh->mTextureCoords[0][i];
               // std::cout << "UV[" << i << "] = (" << uv.x << ", " << uv.y << ")\n";
            }
        } else {
            //std::cout << "Mesh has NO UVs\n";
        }
        //std::cout << "Mesh has " << mesh->GetNumUVChannels() << " UV channels\n";
        return out;
    }

    GLuint loadFirstTexture(aiMaterial* mat, std::initializer_list<aiTextureType> types){
        for(auto type : types){
            if(mat->GetTextureCount(type) > 0){
                aiString str;
                mat->GetTexture(type, 0, &str);
                std::string p = str.C_Str();
                std::string resolved = resolvePath(p);

                std::cout << "Texture found: " << p << " -> resolved path: " << resolved << std::endl;

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
    //===================================================anim

    public:

    void setAnimationLoopRange(float startTimeSec, float endTimeSec) {
        if (!scene_ || scene_->mNumAnimations == 0 || currentAnimIndex_ < 0 || currentAnimIndex_ >= (int)scene_->mNumAnimations) {
            std::cerr << "setAnimationLoopRange: Model neobsahuje animace nebo je neplatny index.\n";
            loopRangeActive_ = false;
            return;
        }

        const aiAnimation* anim = scene_->mAnimations[currentAnimIndex_];
        float tps = (anim->mTicksPerSecond != 0.0f) ? (float)anim->mTicksPerSecond : 25.0f;
        float duration = (float)anim->mDuration;

        // Převod sekund na Assimp "ticks"
        loopStartTicks_ = startTimeSec * tps;
        loopEndTicks_ = endTimeSec * tps;

        // Kontrola platnosti rozsahu a délky animace
        if (loopStartTicks_ >= loopEndTicks_ || loopEndTicks_ > duration || loopStartTicks_ < 0.0f) {
            std::cerr << "setAnimationLoopRange: bad range: [" << startTimeSec << ", " << endTimeSec << "]. Používám celou animaci.\n";
            loopRangeActive_ = false;
            return;
        }

        loopRangeActive_ = true;
       // std::cout << "range anim (in s): [" << startTimeSec << ", " << endTimeSec << "]\n";
    }

    // Metoda pro vypnutí smyčky v rozsahu a návrat k celé animaci
    void disableAnimationLoopRange() {
        loopRangeActive_ = false;
    }
    // lighting
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
        glUseProgram(0);
    }

    // --- animation control ---
    void playAnimationByIndex(int idx) {
        if (!scene_ || idx < 0 || idx >= (int)scene_->mNumAnimations) {
            std::cerr << "playAnimationByIndex: invalid index " << idx << "\n";
            return;
        }

        currentAnimIndex_ = idx;
        animPlaying_ = true;

        const aiAnimation* anim = scene_->mAnimations[idx];
        std::cout << "Playing animation index " << idx << " with "
                  << anim->mNumChannels << " channels:\n";
        for (unsigned i = 0; i < anim->mNumChannels; i++) {
            std::cout << "  Channel[" << i << "]: "
                      << anim->mChannels[i]->mNodeName.C_Str() << "\n";
        }
    }
    void playAnimationByName(const std::string& name) {
        if(!scene_) return;
        for(unsigned i=0;i<scene_->mNumAnimations;i++){
            if(name == scene_->mAnimations[i]->mName.C_Str()){
                currentAnimIndex_ = i;
                animPlaying_ = true;
                return;
            }
        }
        std::cerr << "Animace '"<<name<<"' not found\n";
    }
    void stopAnimation(){ animPlaying_ = false; }
    int numAnimations() const { return scene_? scene_->mNumAnimations : 0; }
    std::string animationName(int idx) const {
        if(!scene_||idx<0||idx>=(int)scene_->mNumAnimations) return "";
        return scene_->mAnimations[idx]->mName.C_Str();
    }

    // call each frame with current time in seconds to update skeleton
    void updateAnimation(float timeSec){
        if(!scene_ || !animPlaying_ || scene_->mNumAnimations == 0){
            // stávající fallback pro neanimované modely nebo zastavenou animaci
            for(auto &b : bones_){
                b.finalTransform = glm::mat4(1.0f); // Jednodušší identita pro statické (neanimované) kosti
            }
            if(bones_.empty()){
                BoneInfo bi;
                bi.finalTransform = glm::mat4(1.0f);
                bones_.push_back(bi);
            }
            return;
        }

        const aiAnimation* anim = scene_->mAnimations[currentAnimIndex_];
        float tps = (anim->mTicksPerSecond != 0.0f) ? (float)anim->mTicksPerSecond : 25.0f;
        float ticks = timeSec * tps;
        float animTime;

        if (loopRangeActive_) {
            // --- NOVÁ LOGIKA PRO LOOP V ROZSAHU ---
            float rangeDuration = loopEndTicks_ - loopStartTicks_;

            // 1. Zjisti pozici v rámci rozsahu (cyklicky)
            float ticksInRange = fmod(ticks - loopStartTicks_, rangeDuration);

            // 2. Přesuň tuto pozici zpět na skutečnou pozici v rámci celé animace
            animTime = loopStartTicks_ + ticksInRange;

        } else {
            // --- PŮVODNÍ LOGIKA PRO CELOU ANIMACI (cyklus od 0 po celou délku) ---
            animTime = fmod(ticks, (float)anim->mDuration);
        }

        readNodeHierarchy(animTime, scene_->mRootNode, glm::mat4(1.0f));
    }
    // ---------- math helpers for animation ----------
    static glm::mat4 aiMatToGlm(const aiMatrix4x4& m) {
        glm::mat4 out;
        out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
        out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
        out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
        out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
        return out;
    }

    static glm::quat aiQuatToGlm(const aiQuaternion& q) {
        return glm::quat(q.w, q.x, q.y, q.z);
    }

    static glm::vec3 aiVecToGlm(const aiVector3D& v) {
        return glm::vec3(v.x,v.y,v.z);
    }

    const aiNodeAnim* findNodeAnim(const aiAnimation* anim, const std::string& name) {
        for (unsigned int i=0;i<anim->mNumChannels;i++) {
            if (std::string(anim->mChannels[i]->mNodeName.C_Str()) == name) {
                return anim->mChannels[i];
            }
        }
        return nullptr;
    }

    glm::vec3 interpolatePosition(float time, const aiNodeAnim* channel) {
        if(channel->mNumPositionKeys == 1)
            return aiVecToGlm(channel->mPositionKeys[0].mValue);
        for(unsigned int i=0;i<channel->mNumPositionKeys-1;i++) {
            if(time < (float)channel->mPositionKeys[i+1].mTime) {
                float t1 = (float)channel->mPositionKeys[i].mTime;
                float t2 = (float)channel->mPositionKeys[i+1].mTime;
                float factor = (time - t1) / (t2 - t1);
                auto start = aiVecToGlm(channel->mPositionKeys[i].mValue);
                auto end   = aiVecToGlm(channel->mPositionKeys[i+1].mValue);
                return glm::mix(start, end, factor);
            }
        }
        return aiVecToGlm(channel->mPositionKeys[channel->mNumPositionKeys-1].mValue);
    }

    glm::quat interpolateRotation(float time, const aiNodeAnim* channel) {
        if(channel->mNumRotationKeys == 1)
            return aiQuatToGlm(channel->mRotationKeys[0].mValue);
        for(unsigned int i=0;i<channel->mNumRotationKeys-1;i++) {
            if(time < (float)channel->mRotationKeys[i+1].mTime) {
                float t1 = (float)channel->mRotationKeys[i].mTime;
                float t2 = (float)channel->mRotationKeys[i+1].mTime;
                float factor = (time - t1) / (t2 - t1);
                auto start = aiQuatToGlm(channel->mRotationKeys[i].mValue);
                auto end   = aiQuatToGlm(channel->mRotationKeys[i+1].mValue);
                return glm::slerp(start, end, factor);
            }
        }
        return aiQuatToGlm(channel->mRotationKeys[channel->mNumRotationKeys-1].mValue);
    }

    glm::vec3 interpolateScaling(float time, const aiNodeAnim* channel) {
        if(channel->mNumScalingKeys == 1)
            return aiVecToGlm(channel->mScalingKeys[0].mValue);
        for(unsigned int i=0;i<channel->mNumScalingKeys-1;i++) {
            if(time < (float)channel->mNumScalingKeys && time < (float)channel->mScalingKeys[i+1].mTime) {
                float t1 = (float)channel->mScalingKeys[i].mTime;
                float t2 = (float)channel->mScalingKeys[i+1].mTime;
                float factor = (time - t1) / (t2 - t1);
                auto start = aiVecToGlm(channel->mScalingKeys[i].mValue);
                auto end   = aiVecToGlm(channel->mScalingKeys[i+1].mValue);
                return glm::mix(start, end, factor);
            }
        }
        return aiVecToGlm(channel->mScalingKeys[channel->mNumScalingKeys-1].mValue);
    }

    void readNodeHierarchy(float animTime, const aiNode* node, const glm::mat4& parentTransform) {
        std::string nodeName(node->mName.C_Str());
        glm::mat4 nodeTransform = aiMatToGlm(node->mTransformation);

        if(scene_ && scene_->mNumAnimations > 0){
            const aiAnimation* anim = scene_->mAnimations[currentAnimIndex_];
            const aiNodeAnim* channel = findNodeAnim(anim, nodeName);

            if(channel){
                glm::vec3 T = interpolatePosition(animTime, channel);
                glm::quat R = interpolateRotation(animTime, channel);
                glm::vec3 S = interpolateScaling(animTime, channel);

                glm::mat4 trans = glm::translate(glm::mat4(1.0f), T);
                glm::mat4 rot = glm::toMat4(R);
                glm::mat4 scale = glm::scale(glm::mat4(1.0f), S);

                nodeTransform = trans * rot * scale;
            }
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        auto it = boneMapping_.find(nodeName);
        if(it != boneMapping_.end()){
            int index = it->second;
            bones_[index].finalTransform = globalTransform * bones_[index].offset;
        }

        // vždy pokračovat v procházení dětí
        for(unsigned int i = 0; i < node->mNumChildren; i++){
            readNodeHierarchy(animTime, node->mChildren[i], globalTransform);
        }
    }

};
