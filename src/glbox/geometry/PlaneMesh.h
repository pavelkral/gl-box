#ifndef PLANEMESH_H
#define PLANEMESH_H

float indexedPlaneVertices[] = {
    // Pozícia             // Normál             // UV
    25.0f,  -0.5f, 25.0f,  0.0f, 1.0f, 0.0f, 10.0f, 0.0f,
    25.0f,  -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 10.0f, 10.0f,
    -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f,  10.0f,
    -25.0f, -0.5f, 25.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f};

unsigned int planeIndices[] = {0, 1, 2, 0, 2, 3};

#endif // PLANEMESH_H
