#ifndef RANDOM_H
#define RANDOM_H

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <random>


class Random {
public:
    static float Float(float min, float max);
    static glm::vec4 RandomColor();
};

#endif // RANDOM_H
