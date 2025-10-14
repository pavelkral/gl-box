#ifndef TEXTURE_H
#define TEXTURE_H


#include <glad/glad.h>
#include <vector>
#include "stb_image.h"
#include <iostream>

namespace Loader {

struct DepthMap {
    unsigned int fbo;
    unsigned int texture;
    unsigned int width;
    unsigned int height;
};

class Trexture
{
public:
    // ============================================
    // Checker texture
    // ============================================
    static GLuint makeCheckerTex(int size = 256, int checks = 8) {
        std::vector<unsigned char> data(size * size * 3);
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int cx = (x * checks) / size;
                int cy = (y * checks) / size;
                bool odd = ((cx + cy) & 1) != 0;
                unsigned char v = odd ? 230 : 40;
                data[(y * size + x) * 3 + 0] = v;
                data[(y * size + x) * 3 + 1] = odd ? 80 : 200;
                data[(y * size + x) * 3 + 2] = odd ? 80 : 240;
            }
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        return tex;
    }

    // ============================================
    // Standardní 2D texture loader (stb_image)
    // ============================================
    static unsigned int loadTexture(const char* path) {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
        if (data) {
            GLenum format = GL_RGB;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            std::cout << "Texture failed to load at path: " << path << std::endl;
        }

        stbi_image_free(data);
        return textureID;
    }

    // ============================================
    // Shadow map (Depth Map FBO)
    // ============================================
    static DepthMap createDepthMapFBO(unsigned int SHADOW_WIDTH = 4096, unsigned int SHADOW_HEIGHT = 4096) {
        DepthMap shadow;
        shadow.width = SHADOW_WIDTH;
        shadow.height = SHADOW_HEIGHT;

        glGenFramebuffers(1, &shadow.fbo);

        glGenTextures(1, &shadow.texture);
        glBindTexture(GL_TEXTURE_2D, shadow.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Enable hardware depth comparison (optional, for PCF)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow.texture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return shadow;
    }

private:
};

} // namespace Loader

#endif // TEXTURE_H


 // TEXTURE_H
