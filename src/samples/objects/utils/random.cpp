#include "random.h"

float Random::Float(float min, float max) {
    static std::mt19937 mt{std::random_device{}()};
    std::uniform_real_distribution<float> dist(min, max);
    return dist(mt);
}

glm::vec4 Random::RandomColor() {
    return glm::vec4(Float(0.2f, 1.0f), Float(0.2f, 1.0f), Float(0.2f, 1.0f), 1.0f);
}
