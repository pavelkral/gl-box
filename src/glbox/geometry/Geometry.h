#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <cmath>
#include <glm/glm.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Geometry
{
public:
    // ===========================================
    // PLANE
    // ===========================================
    static void generatePlane(float width, float depth, int segX, int segZ,
                              float tileU, float tileV,
                              std::vector<float>& vertices,
                              std::vector<unsigned int>& indices)
    {
        vertices.clear();
        indices.clear();

        float halfW = width * 0.5f;
        float halfD = depth * 0.5f;
        float stepX = width / segX;
        float stepZ = depth / segZ;
        float uvStepX = 1.0f / segX;
        float uvStepZ = 1.0f / segZ;

        for (int z = 0; z <= segZ; z++) {
            for (int x = 0; x <= segX; x++) {
                float posX = -halfW + x * stepX;
                float posZ = -halfD + z * stepZ;

                // Position
                vertices.push_back(posX);
                vertices.push_back(0.0f);
                vertices.push_back(posZ);

                // Normal
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);

                // UV
                vertices.push_back(x * uvStepX * tileU);
                vertices.push_back(z * uvStepZ * tileV);
            }
        }

        // Indices
        for (int z = 0; z < segZ; z++) {
            for (int x = 0; x < segX; x++) {
                unsigned int v1 = z * (segX + 1) + x;
                unsigned int v2 = v1 + 1;
                unsigned int v3 = v1 + (segX + 1);
                unsigned int v4 = v3 + 1;

                indices.push_back(v1);
                indices.push_back(v2);
                indices.push_back(v3);

                indices.push_back(v2);
                indices.push_back(v4);
                indices.push_back(v3);
            }
        }
    }

    // ===========================================
    // SPHERE
    // ===========================================
    static void generateSphere(float radius, int rings, int sectors,
                               std::vector<float>& vertices,
                               std::vector<unsigned int>& indices)
    {
        vertices.clear();
        indices.clear();

        const float R = 1.0f / (rings - 1);
        const float S = 1.0f / (sectors - 1);

        for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < sectors; ++s) {
                float y = sin(-M_PI / 2 + M_PI * r * R);
                float x = cos(2 * M_PI * s * S) * sin(M_PI * r * R);
                float z = sin(2 * M_PI * s * S) * sin(M_PI * r * R);

                // Position
                vertices.push_back(x * radius);
                vertices.push_back(y * radius);
                vertices.push_back(z * radius);

                // Normal
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // UV
                vertices.push_back(s * S);
                vertices.push_back(r * R);
            }
        }

        for (int r = 0; r < rings - 1; ++r) {
            for (int s = 0; s < sectors - 1; ++s) {
                unsigned int v1 = r * sectors + s;
                unsigned int v2 = r * sectors + (s + 1);
                unsigned int v3 = (r + 1) * sectors + (s + 1);
                unsigned int v4 = (r + 1) * sectors + s;

                indices.push_back(v1);
                indices.push_back(v3);
                indices.push_back(v4);

                indices.push_back(v1);
                indices.push_back(v2);
                indices.push_back(v3);
            }
        }
    }

    // ===========================================
    // CUBE
    // ===========================================
    static void generateCube(float size,
                             std::vector<float>& vertices,
                             std::vector<unsigned int>& indices)
    {
        vertices.clear();
        indices.clear();

        float h = size * 0.5f;

        struct Face {
            glm::vec3 normal;
            glm::vec3 v[4];
            glm::vec2 uv[4];
        };

        std::vector<Face> faces = {
                                   // Front
                                   {{0, 0, 1},
                                    {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   // Back
                                   {{0, 0, -1},
                                    {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   // Left
                                   {{-1, 0, 0},
                                    {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   // Right
                                   {{1, 0, 0},
                                    {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   // Top
                                   {{0, 1, 0},
                                    {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   // Bottom
                                   {{0, -1, 0},
                                    {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}},
                                    {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
                                   };

        unsigned int offset = 0;
        for (auto& f : faces) {
            for (int i = 0; i < 4; i++) {
                vertices.push_back(f.v[i].x);
                vertices.push_back(f.v[i].y);
                vertices.push_back(f.v[i].z);

                vertices.push_back(f.normal.x);
                vertices.push_back(f.normal.y);
                vertices.push_back(f.normal.z);

                vertices.push_back(f.uv[i].x);
                vertices.push_back(f.uv[i].y);
            }

            indices.push_back(offset + 0);
            indices.push_back(offset + 1);
            indices.push_back(offset + 2);
            indices.push_back(offset + 0);
            indices.push_back(offset + 2);
            indices.push_back(offset + 3);
            offset += 4;
        }
    }

    // ===========================================
    // LIGHT FRUSTUM
    // ===========================================
    static void generateLightFrustum(float orthoSize, float nearPlane, float farPlane,
                                     std::vector<float>& vertices,
                                     std::vector<unsigned int>& indices)
    {
        vertices.clear();
        indices.clear();

        glm::vec3 c[8] = {
            {-orthoSize, -orthoSize, -nearPlane}, // 0
            { orthoSize, -orthoSize, -nearPlane}, // 1
            { orthoSize,  orthoSize, -nearPlane}, // 2
            {-orthoSize,  orthoSize, -nearPlane}, // 3
            {-orthoSize, -orthoSize, -farPlane},  // 4
            { orthoSize, -orthoSize, -farPlane},  // 5
            { orthoSize,  orthoSize, -farPlane},  // 6
            {-orthoSize,  orthoSize, -farPlane}   // 7
        };

        auto addVertex = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uv) {
            vertices.insert(vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
        };

        glm::vec3 n[6] = {
            {0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
        };

        // Near
        addVertex(c[0], n[0], {0,0}); addVertex(c[1], n[0], {1,0});
        addVertex(c[2], n[0], {1,1}); addVertex(c[3], n[0], {0,1});
        // Far
        addVertex(c[5], n[1], {0,0}); addVertex(c[4], n[1], {1,0});
        addVertex(c[7], n[1], {1,1}); addVertex(c[6], n[1], {0,1});
        // Left
        addVertex(c[4], n[2], {0,0}); addVertex(c[0], n[2], {1,0});
        addVertex(c[3], n[2], {1,1}); addVertex(c[7], n[2], {0,1});
        // Right
        addVertex(c[1], n[3], {0,0}); addVertex(c[5], n[3], {1,0});
        addVertex(c[6], n[3], {1,1}); addVertex(c[2], n[3], {0,1});
        // Top
        addVertex(c[3], n[4], {0,0}); addVertex(c[2], n[4], {1,0});
        addVertex(c[6], n[4], {1,1}); addVertex(c[7], n[4], {0,1});
        // Bottom
        addVertex(c[4], n[5], {0,0}); addVertex(c[5], n[5], {1,0});
        addVertex(c[1], n[5], {1,1}); addVertex(c[0], n[5], {0,1});

        for (int f = 0; f < 6; f++) {
            unsigned int base = f * 4;
            indices.insert(indices.end(), {base, base+1, base+2, base, base+2, base+3});
        }
    }

    // ===========================================
    // SIMPLE STATIC PLANE (example)
    // ===========================================
    static inline float simplePlaneVertices[32] = {
        25.0f,  -0.5f, 25.0f,   0.0f, 1.0f, 0.0f,   10.0f, 0.0f,
        25.0f,  -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   10.0f, 10.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  10.0f,
        -25.0f, -0.5f, 25.0f,   0.0f, 1.0f, 0.0f,   0.0f,  0.0f
    };

    static inline unsigned int simplePlaneIndices[6] = {0, 1, 2, 0, 2, 3};


};

float indexedCubeVertices[] = {
    // Pos               // Norm                // UV
    -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, 0.0f, 0.0f,
    0.5f,  -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, 1.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, 1.0f, 1.0f,
    -0.5f, 0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, 0.0f, 1.0f,

    -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
    0.5f,  -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
    0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
    -0.5f, 0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,

    -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  1.0f, 0.0f,
    -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f,  0.0f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,  -1.0f, 0.0f,  0.0f,  0.0f, 0.0f,

    0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
    0.5f,  -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

    -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  0.0f, 1.0f,
    0.5f,  -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  1.0f, 1.0f,
    0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,  1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,  0.0f, 0.0f,

    -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
    0.5f,  0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f};

unsigned int cubeIndices[] = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                              8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                              16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};


#endif // GEOMETRY_H
