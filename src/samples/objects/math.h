#ifndef MATH_H
#define MATH_H
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Math{

bool checkAABB(const glm::vec3 &bPos, const glm::vec3 &bScale, const glm::vec3 &spherePos, float r) {

    float halfW = bScale.x * 0.5f;
    float halfH = bScale.y * 0.5f;
    return (spherePos.x + r > bPos.x - halfW && spherePos.x - r < bPos.x + halfW &&
            spherePos.y + r > bPos.y - halfH && spherePos.y - r < bPos.y + halfH);
}

glm::vec3 reflectVector(const glm::vec3 &v, const glm::vec3 &normal) {
    return v - 2.0f * glm::dot(v, normal) * normal;
}

bool checkBoxVsBoxAABB(const glm::vec3 &boxPos, const glm::vec3 &boxScale, const glm::vec3 &otherPos, const glm::vec3 &otherScale) {
    float hW1 = boxScale.x * 0.5f; float hH1 = boxScale.y * 0.5f;
    float hW2 = otherScale.x * 0.5f; float hH2 = otherScale.y * 0.5f;
    return (boxPos.x + hW1 > otherPos.x - hW2 && boxPos.x - hW1 < otherPos.x + hW2 &&
            boxPos.y + hH1 > otherPos.y - hH2 && boxPos.y - hH1 < otherPos.y + hH2);
}

};


#endif // MATH_H
