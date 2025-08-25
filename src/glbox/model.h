#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string>
#include "StaticMesh.h"
#include "Material.h"
#include "Transform.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model {
public:
    Model(const std::string& path, Material* material);
    void Draw(  const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix);
    void DrawForShadow(unsigned int depthShaderID, const glm::mat4& lightSpaceMatrix) const;
     std::vector<StaticMesh> meshes;
    Material* material;
     Transform transform;
private:

    std::string directory;
    std::vector<Texture> textures_loaded;

    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    StaticMesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
};

#endif
