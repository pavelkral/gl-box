#ifndef MESHFACTORY_H
#define MESHFACTORY_H

#include "mesh.h"

namespace MeshFactory {

   inline std::unique_ptr<Mesh> createCube() {
        std::vector<float> vertices = {
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
        };
        std::vector<unsigned int> indices = {
            0,1,2, 2,3,0, 4,5,6, 6,7,4, 0,1,5, 5,4,0, 2,3,7, 7,6,2, 0,3,7, 7,4,0, 1,2,6, 6,5,1
        };
        return std::make_unique<Mesh>(vertices, indices);
    }

   inline std::unique_ptr<Mesh> createSphere(float radius = 1.0f, int latSeg = 16, int longSeg = 16) {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        for (int y=0;y<=latSeg;y++){
            float theta = (float)y * glm::pi<float>() / latSeg;
            for (int x=0;x<=longSeg;x++){
                float phi = (float)x * 2.0f * glm::pi<float>() / longSeg;
                vertices.push_back(radius * cos(phi) * sin(theta));
                vertices.push_back(radius * cos(theta));
                vertices.push_back(radius * sin(phi) * sin(theta));
            }
        }
        for (int y=0;y<latSeg;y++){
            for (int x=0;x<longSeg;x++){
                int a = y*(longSeg+1)+x;
                int b = a+longSeg+1;
                indices.push_back((unsigned int)a);
                indices.push_back((unsigned int)b);
                indices.push_back((unsigned int)(a+1));
                indices.push_back((unsigned int)b);
                indices.push_back((unsigned int)(b+1));
                indices.push_back((unsigned int)(a+1));
            }
        }
        return std::make_unique<Mesh>(vertices, indices);
    }
}
#endif // MESHFACTORY_H
