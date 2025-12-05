#ifndef MATH_H
#define MATH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Math{

    bool checkAABB(const glm::vec3& bPos, const glm::vec3& bScale, const glm::vec3& spherePos, float r);

    glm::vec3 reflectVector(const glm::vec3& v, const glm::vec3& normal);

    bool checkBoxVsBoxAABB(const glm::vec3& boxPos, const glm::vec3& boxScale, const glm::vec3& otherPos, const glm::vec3& otherScale);

};

#endif // MATH_H
