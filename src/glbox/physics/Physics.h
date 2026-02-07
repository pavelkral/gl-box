#ifndef PHYSICS_H
#define PHYSICS_H
//#include "../glbox/geometry/Geometry.h"
#include "Raycast.h"
#include "../StaticMesh.h"

bool PerformRaycast(const Ray& ray,
                    const Octree& sceneOctree,
                    const std::map<StaticMesh*, glm::mat4>& modelMatrices,
                    RaycastHit& outHit)
{
    outHit.hit = false;
    outHit.distance = FLT_MAX;
    outHit.object = nullptr;

    // 1. Získáme ze stromu jen relevantní objekty
    std::vector<StaticMesh*> potentialHits;
    sceneOctree.Query(ray, potentialHits);

    if (potentialHits.empty()) {
        return false;
    }

    // 2. Teď provedeme test "brute-force", ALE UŽ JEN NA TĚCH PÁR KANDIDÁTECH
    for (StaticMesh* mesh : potentialHits) {

        // Bezpečné nalezení matice v mapě
        auto it = modelMatrices.find(mesh);
        if (it == modelMatrices.end()) {
            // Tento mesh nemá z nějakého důvodu matici v mapě, přeskočíme ho
            std::cerr << "err: Raycast not find matrix for mesh!" << std::endl;
            continue;
        }

        // Použijeme matici z parametru
        const glm::mat4& modelMatrix = it->second;

        // Znovu spočítáme world AABB pro přesný test
        // (Alternativa: Octree by mohl vracet i AABB, se kterým byl objekt vložen)
        BoxCollider worldAABB = mesh->localAABB.GetTransformed(modelMatrix);

        float t; // Vzdálenost zásahu
        if (worldAABB.Intersects(ray, t)) {

            // TODO: Zde by měl být přesný test proti trojúhelníkům.
            // Prozatím bereme zásah AABB jako finální zásah.

            // 3. Je tento zásah blíž než ten předchozí?
            if (t < outHit.distance) {
                outHit.hit = true;
                outHit.distance = t;
                outHit.point = ray.origin + ray.direction * t;
                outHit.object = mesh;
            }
        }
    }

    return outHit.hit;
}

#endif // PHYSICS_H
