#ifndef RAYCAST_H
#define RAYCAST_H
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <map>
#include <set>
#include <cfloat>
#include <iostream>

// Forward deklarace tvé třídy. Díky tomu nemusíme #include "StaticMesh.h"
// přímo zde na vrchu a vyhneme se cyklickým závislostem.
class StaticMesh;

//=========================================================================================
// 1. DEFINICE PAPRSKU (Ray)
//=========================================================================================
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;

    Ray(const glm::vec3& o, const glm::vec3& d)
        : origin(o), direction(glm::normalize(d)) {}
};

//=========================================================================================
// 2. DEFINICE BOUNDING BOXU (BoxCollider)
//=========================================================================================
class BoxCollider {
public:
    glm::vec3 min;
    glm::vec3 max;

    BoxCollider()
        : min(glm::vec3(FLT_MAX)), max(glm::vec3(-FLT_MAX)) {}

    BoxCollider(const glm::vec3& min, const glm::vec3& max)
        : min(min), max(max) {}

    /**
     * Vypočítá AABB z daného seznamu vrcholů.
     * Očekává, že 'stride' je počet floatů na jeden vrchol.
     */
    void CalculateFromVertices(const std::vector<float>& vertices, int stride) {
        min = glm::vec3(FLT_MAX);
        max = glm::vec3(-FLT_MAX);

        if (stride <= 0 || vertices.empty()) {
            min = glm::vec3(0.0f);
            max = glm::vec3(0.0f);
            return;
        }

        for (size_t i = 0; i < vertices.size(); i += stride) {
            min.x = glm::min(min.x, vertices[i + 0]);
            min.y = glm::min(min.y, vertices[i + 1]);
            min.z = glm::min(min.z, vertices[i + 2]);

            max.x = glm::max(max.x, vertices[i + 0]);
            max.y = glm::max(max.y, vertices[i + 1]);
            max.z = glm::max(max.z, vertices[i + 2]);
        }
    }

    /**
     * Testuje průnik paprsku s tímto AABB (Slab Test).
     * Vrací 'true' při zásahu a ukládá vzdálenost do 't'.
     */
    bool Intersects(const Ray& ray, float& t) const {
        // Vyhneme se dělení nulou
        glm::vec3 invDir;
        invDir.x = (ray.direction.x == 0.0f) ? FLT_MAX : (1.0f / ray.direction.x);
        invDir.y = (ray.direction.y == 0.0f) ? FLT_MAX : (1.0f / ray.direction.y);
        invDir.z = (ray.direction.z == 0.0f) ? FLT_MAX : (1.0f / ray.direction.z);

        glm::vec3 t1 = (min - ray.origin) * invDir;
        glm::vec3 t2 = (max - ray.origin) * invDir;

        glm::vec3 tMinVec = glm::min(t1, t2);
        glm::vec3 tMaxVec = glm::max(t1, t2);

        float tMin = glm::max(tMinVec.x, glm::max(tMinVec.y, tMinVec.z));
        float tMax = glm::min(tMaxVec.x, glm::min(tMaxVec.y, tMaxVec.z));

        if (tMax < 0 || tMin > tMax) {
            t = FLT_MAX;
            return false;
        }

        t = (tMin < 0.0f) ? tMax : tMin;
        return t > 0.0f; // Chceme jen zásahy před námi
    }

    /**
     * Testuje, zda se tento AABB protíná s jiným AABB.
     */
    bool Intersects(const BoxCollider& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    /**
     * Testuje, zda tento AABB plně obsahuje jiný AABB.
     */
    bool Contains(const BoxCollider& other) const {
        return (other.min.x >= min.x && other.max.x <= max.x) &&
               (other.min.y >= min.y && other.max.y <= max.y) &&
               (other.min.z >= min.z && other.max.z <= max.z);
    }

    /**
     * Vrátí nový AABB, který obaluje tento AABB po transformaci maticí 'm'.
     */
    BoxCollider GetTransformed(const glm::mat4& m) const {
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };

        BoxCollider transformedAABB;
        transformedAABB.min = glm::vec3(FLT_MAX);
        transformedAABB.max = glm::vec3(-FLT_MAX);

        for (int i = 0; i < 8; ++i) {
            glm::vec3 transformedCorner = glm::vec3(m * glm::vec4(corners[i], 1.0f));
            transformedAABB.min = glm::min(transformedAABB.min, transformedCorner);
            transformedAABB.max = glm::max(transformedAABB.max, transformedCorner);
        }
        return transformedAABB;
    }
};

//=========================================================================================
// 3. DEFINICE VÝSLEDKU ZÁSAHU (RaycastHit)
//=========================================================================================
struct RaycastHit {
    bool hit = false;
    float distance = FLT_MAX;
    glm::vec3 point;
    StaticMesh* object = nullptr; // Pointer na zasažený objekt
};

//=========================================================================================
// 4. DEFINICE UZLU PRO OCTREE (OctreeNode)
//=========================================================================================
class OctreeNode {
public:
    BoxCollider bounds;
    OctreeNode* children[8];
    std::vector<StaticMesh*> objects;
    bool isLeaf;

    OctreeNode(const BoxCollider& b) : bounds(b), isLeaf(true) {
        for (int i = 0; i < 8; ++i) {
            children[i] = nullptr;
        }
    }

    ~OctreeNode() {
        for (int i = 0; i < 8; ++i) {
            delete children[i];
        }
    }

    /**
     * Rozdělí tento uzel na 8 potomků.
     */
    void Subdivide() {
        glm::vec3 center = (bounds.min + bounds.max) * 0.5f;

        children[0] = new OctreeNode(BoxCollider(bounds.min, center));
        children[1] = new OctreeNode(BoxCollider(glm::vec3(center.x, bounds.min.y, bounds.min.z), glm::vec3(bounds.max.x, center.y, center.z)));
        children[2] = new OctreeNode(BoxCollider(glm::vec3(bounds.min.x, center.y, bounds.min.z), glm::vec3(center.x, bounds.max.y, center.z)));
        children[3] = new OctreeNode(BoxCollider(glm::vec3(bounds.min.x, bounds.min.y, center.z), glm::vec3(center.x, center.y, bounds.max.z)));
        children[4] = new OctreeNode(BoxCollider(glm::vec3(center.x, center.y, bounds.min.z), glm::vec3(bounds.max.x, bounds.max.y, center.z)));
        children[5] = new OctreeNode(BoxCollider(glm::vec3(center.x, bounds.min.y, center.z), glm::vec3(bounds.max.x, center.y, bounds.max.z)));
        children[6] = new OctreeNode(BoxCollider(glm::vec3(bounds.min.x, center.y, center.z), glm::vec3(center.x, bounds.max.y, bounds.max.z)));
        children[7] = new OctreeNode(BoxCollider(center, bounds.max));

        isLeaf = false;
    }
};

//=========================================================================================
// 5. DEFINICE OCTREE
//=========================================================================================
class Octree {
public:
    OctreeNode* root;
    int maxObjectsPerNode;
    int maxDepth;

    // Mapa pro ukládání AABB ke každému objektu, abychom je nemuseli znovu počítat
    std::map<StaticMesh*, BoxCollider> objectAABBs;

    Octree(const BoxCollider& rootBounds, int maxObj = 8, int maxD = 10)
        : maxObjectsPerNode(maxObj), maxDepth(maxD) {
        root = new OctreeNode(rootBounds);
    }

    ~Octree() {
        delete root;
    }

    /**
     * Vymaže celý strom a znovu ho postaví.
     */
    void Clear() {
        delete root;
        root = new OctreeNode(BoxCollider()); // Vytvoří prázdný root
        objectAABBs.clear();
    }

    /**
     * Aktualizuje AABB pro celý strom (pokud se scéna změní).
     * Pro dynamický strom je lepší implementovat Remove/Update,
     * ale pro statickou scénu stačí Clear() a poté Build().
     */
    void Build(const std::map<StaticMesh*, BoxCollider>& allWorldAABBs) {
        // 1. Najdi celkový AABB scény
        BoxCollider sceneBounds;
        for (const auto& pair : allWorldAABBs) {
            sceneBounds.min = glm::min(sceneBounds.min, pair.second.min);
            sceneBounds.max = glm::max(sceneBounds.max, pair.second.max);
        }

        // Zvětšíme AABB o 10%, abychom se vyhnuli problémům na hranách
        glm::vec3 size = sceneBounds.max - sceneBounds.min;
        sceneBounds.min -= size * 0.05f;
        sceneBounds.max += size * 0.05f;

        // 2. Vymaž starý strom a nastav nový root
        delete root;
        root = new OctreeNode(sceneBounds);
        objectAABBs.clear();

        // 3. Vlož všechny objekty
        for (const auto& pair : allWorldAABBs) {
            Insert(pair.first, pair.second);
        }
    }


    /**
     * Vloží objekt s jeho AABB do stromu.
     */
    void Insert(StaticMesh* object, const BoxCollider& worldAABB) {
        objectAABBs[object] = worldAABB; // Uložíme si AABB
        InsertRecursive(root, object, worldAABB, 0);
    }

    /**
     * Vrátí (přes std::vector) seznam všech objektů,
     * jejichž AABB by mohl paprsek protnout.
     */
    void Query(const Ray& ray, std::vector<StaticMesh*>& potentialHits) const{
        potentialHits.clear();
        std::set<StaticMesh*> hitSet; // Set, aby se zabránilo duplicitám
        QueryRecursive(root, ray, hitSet);
        potentialHits.assign(hitSet.begin(), hitSet.end());
    }

private:
    /**
     * Najde index potomka (0-7), do kterého AABB plně spadá.
     * Vrátí -1, pokud AABB překrývá více potomků (nebo žádného).
     */
    int GetChildIndexForAABB(OctreeNode* node, const BoxCollider& worldAABB) {
        int index = -1;
        for (int i = 0; i < 8; ++i) {
            if (node->children[i] == nullptr) return -1; // Nemělo by nastat, pokud !isLeaf

            if (node->children[i]->bounds.Contains(worldAABB)) {
                // Pokud už jsme našli jeden (index != -1), znamená to, že
                // AABB je obsažen ve dvou, což by nemělo být (ale pro jistotu).
                // Důležitější je, že pokud sedí v JEDNOM, vrátíme jeho index.
                if (index != -1) {
                    return -1; // Spadá do více než jednoho (divné)
                }
                index = i;
            }
        }
        return index;
    }


    void InsertRecursive(OctreeNode* node, StaticMesh* object, const BoxCollider& worldAABB, int depth) {
        // Pokud se AABB objektu ani nedotýká tohoto uzlu, končíme
        if (!node->bounds.Intersects(worldAABB)) {
            return;
        }

        if (node->isLeaf) {
            // Jsme v listu, přidáme objekt sem
            node->objects.push_back(object);

            // Pokud je uzel přeplněný a nedosáhli jsme max. hloubky, rozdělíme ho
            if (node->objects.size() > maxObjectsPerNode && depth < maxDepth) {
                node->Subdivide();

                // Nyní přesuneme všechny objekty z tohoto (teď už rodičovského)
                // uzlu dolů do nových potomků.
                std::vector<StaticMesh*> tempObjects = std::move(node->objects);
                node->objects.clear();

                for (StaticMesh* obj : tempObjects) {
                    const BoxCollider& objAABB = objectAABBs.at(obj); // Vezmeme si uložené AABB

                    // Zkusíme najít ideálního potomka
                    int index = GetChildIndexForAABB(node, objAABB);
                    if (index != -1) {
                        // AABB se vejde přesně do jednoho potomka
                        InsertRecursive(node->children[index], obj, objAABB, depth + 1);
                    } else {
                        // AABB překrývá hranice, musí zůstat v tomto rodičovském uzlu
                        node->objects.push_back(obj);
                    }
                }
            }
        } else {
            // Toto je větev (branch node), ne list.
            // Zkusíme objekt poslat dál do potomků.
            int index = GetChildIndexForAABB(node, worldAABB);
            if (index != -1) {
                // Objekt se vejde přesně do jednoho potomka
                InsertRecursive(node->children[index], object, worldAABB, depth + 1);
            } else {
                // Objekt překrývá hranice potomků, musí zůstat zde
                node->objects.push_back(object);
            }
        }
    }

    void QueryRecursive(OctreeNode* node, const Ray& ray, std::set<StaticMesh*>& hitSet) const {
        float t;
        // Pokud paprsek vůbec neprotíná tento uzel, končíme
        if (!node->bounds.Intersects(ray, t)) {
            return;
        }

        // Přidáme všechny objekty, které jsou uloženy PŘÍMO v tomto uzlu
        // (Buď je to list, nebo jsou to objekty překrývající hranice)
        for (StaticMesh* obj : node->objects) {
            hitSet.insert(obj);
        }

        // Pokud to NENÍ list, pokračujeme rekurzivně k dětem
        if (!node->isLeaf) {
            for (int i = 0; i < 8; ++i) {
                QueryRecursive(node->children[i], ray, hitSet);
            }
        }
    }
};
#endif // RAYCAST_H
