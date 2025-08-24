#ifndef SCENEOBJECTS_H
#define SCENEOBJECTS_H
// SceneObject.h


#include "Transform.h"
#include "Material.h"
#include "StaticMesh.h"

// Třída pro jeden objekt ve scéně, která je kontejnerem pro komponenty
class SceneObject {
public:
    Transform transform; // Komponenta Transform
    StaticMesh* mesh;    // Ukazatel na síť (mesh)
    Material* material;  // Ukazatel na materiál

    // Konstruktor
    SceneObject(StaticMesh* mesh, Material* material) : mesh(mesh), material(material) {}

    // Metoda pro vykreslení objektu
    void Draw(unsigned int shaderProgram, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        // Použití shaderu
        glUseProgram(shaderProgram);

        // Odeslání uniform proměnných do shaderu
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(transform.GetModelMatrix()));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        // Nastavení textur
        material->UseTextures();

        // Vykreslení sítě
        mesh->Draw();
    }

    // Speciální metoda pro vykreslení hloubkové mapy (depth map)
    void DrawForShadow(unsigned int depthShader) const {
        glUseProgram(depthShader);
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "model"), 1, GL_FALSE, glm::value_ptr(transform.GetModelMatrix()));
        mesh->Draw();
    }
};
#endif // SCENEOBJECTS_H
