#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#include "Transform.h"
//#include "ProceduralMesh.h"
#include "StaticMesh.h"
#include "Model.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class SceneObject {

private:

    StaticMesh* statiMesh = nullptr;
    ModelFBX* model = nullptr;

public:
    Transform transform;

    SceneObject() = default;

    SceneObject(StaticMesh* statiMesh) : statiMesh(statiMesh) {}

    SceneObject(ModelFBX* model) : model(model) {}

    SceneObject(const Transform& initialTransform,StaticMesh* sMesh = nullptr, ModelFBX* fbxModel = nullptr)
        : transform(initialTransform),  statiMesh(sMesh), model(fbxModel) {}

    StaticMesh* getStaticMesh() const { return statiMesh; }
    ModelFBX* getModel() const { return model; }


    void setStaticMesh(StaticMesh* newStatiMesh) {
        statiMesh = newStatiMesh;
    }
    void setModel(ModelFBX* newModel) {
        model = newModel;
    }

    void Draw( const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
              const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir, const glm::vec3& lightCol) const {
          const glm::mat4 modelMatrix = transform.GetModelMatrix();

        if (statiMesh) {
            statiMesh->Draw(modelMatrix, view, proj,
                             cameraPos,envCubemap,shadowMap,
                             lightSpaceMatrix, lightDir, lightCol);
        } else if (model) {
            model->draw( view, proj,  cameraPos);
        }
    }

    void DrawForShadow(unsigned int depthShaderID, const glm::mat4& lightSpaceMatrix) const {
        const glm::mat4 modelMatrix = transform.GetModelMatrix();

        glUseProgram(depthShaderID);
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

        if (statiMesh) {
            statiMesh->DrawForShadow(depthShaderID, modelMatrix, lightSpaceMatrix);
        } else if (model) {
            model->DrawForShadow(depthShaderID, lightSpaceMatrix);
        }

        glUseProgram(0);
    }

    void Update() {
    }
};

#endif // SCENEOBJECT_H
