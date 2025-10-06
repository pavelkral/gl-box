// Sphere.h
#ifndef SPHERE_H
#define SPHERE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <iostream> // pro createShader error

class Sphere
{
private:
    unsigned int cubeShader; // Uber Shader

    static unsigned int sphereVAO, sphereVBO, sphereEBO;
    static unsigned int indexCount;

    // --- SHADERY INTEGROVÁNY PŘÍMO VE TŘÍDĚ ---
    static const char* cubeVS;
    static const char* cubeFS;
    // ------------------------------------------

    static unsigned int createShader(const char* vs, const char* fs);
    static void renderSphere();

public:
    Sphere() : cubeShader(0) {}

    void init();
    void setMaterial(int mode, const glm::vec3& color, float alpha,
                     float ior, float fresnelPwr, float reflectStr);
    void draw(unsigned int envCubemap, const glm::mat4& model, const glm::mat4& view,
              const glm::mat4& projection, const glm::vec3& cameraPos, bool transparent) const;
};

// Vnější definice statických proměnných
// Vnější definice statických proměnných
unsigned int Sphere::sphereVAO = 0;
unsigned int Sphere::sphereVBO = 0;
unsigned int Sphere::sphereEBO = 0;
unsigned int Sphere::indexCount = 0;

// *** DEFINICE SHADERŮ ***
const char* Sphere::cubeVS = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
out vec3 Normal;
out vec3 Position;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    Normal = mat3(transpose(inverse(model))) * aNormal;
    Position = vec3(model * vec4(aPos,1.0));
    gl_Position = projection * view * vec4(Position,1.0);
}
)glsl";

// NOVÝ FRAGMENT SHADER PRO SPRÁVNOU LOGIKU CHROMU (Čistá reflexe)
const char* Sphere::cubeFS = R"glsl(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube environmentMap;

// Uber Parametry:
uniform int renderMode;            // 0: Reflexe/Sklo (default), 1: Cista Barva (difuzni)
uniform vec3 materialColor;        // Barva pro difuzni nebo tonovani reflexe/skla
uniform float alpha;               // Globalni pruhlednost (pro Blending)

// Reflexe/Sklo Parametry (Mode 0):
uniform float refractionIndex;     // Index lomu (1.0/IOR)
uniform float fresnelPower;        // Mocnitel pro Schlickovu aproximaci (typicky 5.0)
uniform float reflectionStrength;  // Sila odrazu (mix)

void main()
{
    vec3 N = normalize(Normal);
    vec3 I = normalize(Position - cameraPos);

    vec3 finalColor = vec3(0.0);
    float finalAlpha = alpha;

    if (renderMode == 1) // CISTA BARVA (Difuzni/Matna)
    {
        finalColor = materialColor;
    }
    else // renderMode == 0 (Reflexe/Sklo/Chrom)
    {
        // --- 1. REFLEXE (Odraz) ---
        vec3 reflectedRay = reflect(I, N);
        vec3 reflectedColor = texture(environmentMap, reflectedRay).rgb;

        // --- 2. FRESNELŮV EFEKT ---
        float R0 = 0.04;
        float fresnel = R0 + (1.0 - R0) * pow(1.0 - max(0.0, dot(-I, N)), fresnelPower);

        // Finalni mix faktor, ktery bere v potaz uzivatelskou Reflection Strength
        float finalMix = mix(fresnel, reflectionStrength, 0.5);

        // ----------------------------------------------------
        // KRITICKÁ LOGIKA PRO CHROM/KOVOVÉ MATERIÁLY
        // ----------------------------------------------------

        if (alpha >= 0.99 && reflectionStrength >= 0.99) // Čistý CHROME / KOV
        {
            // Plná, čistá reflexe.
            finalColor = reflectedColor;
        }
        else // SKLO / Tónovaný reflexní dielektrikum
        {
            // --- 3. REFRAKCE (Lámání světla) ---
            vec3 refractedRay = refract(I, N, refractionIndex);

            // Zpracování totalniho odrazu
            if (dot(refractedRay, refractedRay) == 0.0) {
                refractedRay = reflect(I, N);
            } else {
                refractedRay = normalize(refractedRay);
            }
            vec3 refractedColor = texture(environmentMap, refractedRay).rgb;

            // Smichani lomu (refraction) a odrazu (reflection) pomoci Fresnelova jevu
            finalColor = mix(refractedColor, reflectedColor, finalMix);
        }

        // Aplikace barvy pro tonovani (pro chrom i sklo)
        finalColor *= materialColor;
    }

    // Tonemapping a Gamma korekce (aplikuje se vzdy)
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, finalAlpha);
}
)glsl";
// *** KONEC DEFINICE SHADERŮ ***

// IMPLEMENTACE METOD

inline unsigned int Sphere::createShader(const char* vs, const char* fs)
{
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vertex, 1, &vs, NULL); glCompileShader(vertex);
    int success; char info[512]; glGetShaderiv(vertex, GL_COMPILE_STATUS, &success); if (!success) { glGetShaderInfoLog(vertex, 512, NULL, info); std::cout << "Vertex: " << info << "\n"; }

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(frag, 1, &fs, NULL); glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success); if (!success) { glGetShaderInfoLog(frag, 512, NULL, info); std::cout << "Fragment: " << info << "\n"; }

    unsigned int program = glCreateProgram(); glAttachShader(program, vertex); glAttachShader(program, frag); glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success); if (!success) { glGetProgramInfoLog(program, 512, NULL, info); std::cout << "Link: " << info << "\n"; }

    glDeleteShader(vertex); glDeleteShader(frag);
    return program;
}

inline void Sphere::renderSphere()
{
    float M_PI = 3.141592654f; // Použijeme f pro float literál
    if (sphereVAO == 0)
    {
        // ... (zde zkopírujte celou logiku pro výpočet geometrie a VAO/VBO/EBO) ...
        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSeg = (float)x / X_SEGMENTS;
                float ySeg = (float)y / Y_SEGMENTS;
                float xPos = cos(xSeg * 2.0f * M_PI) * sin(ySeg * M_PI);
                float yPos = cos(ySeg * M_PI);
                float zPos = sin(xSeg * 2.0f * M_PI) * sin(ySeg * M_PI);
                positions.push_back(glm::vec3(xPos, yPos, zPos));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x)
            {
                unsigned int a = y * (X_SEGMENTS + 1) + x;
                unsigned int b = (y + 1) * (X_SEGMENTS + 1) + x;
                unsigned int c = (y + 1) * (X_SEGMENTS + 1) + x + 1;
                unsigned int d = y * (X_SEGMENTS + 1) + x + 1;

                indices.push_back(a); indices.push_back(b); indices.push_back(d);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
        indexCount = indices.size();

        std::vector<float> data;
        for (size_t i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            data.push_back(normals[i].x);
            data.push_back(normals[i].y);
            data.push_back(normals[i].z);
        }

        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);

        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

inline void Sphere::init()
{
    cubeShader = createShader(cubeVS, cubeFS);
    glUseProgram(cubeShader);
    glUniform1i(glGetUniformLocation(cubeShader, "environmentMap"), 0);
}

inline void Sphere::setMaterial(int mode, const glm::vec3& color, float alpha,
                                float ior, float fresnelPwr, float reflectStr)
{
    glUseProgram(cubeShader);
    glUniform1i(glGetUniformLocation(cubeShader, "renderMode"), mode);
    glUniform3fv(glGetUniformLocation(cubeShader, "materialColor"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(cubeShader, "alpha"), alpha);

    if (mode == 0) {
        glUniform1f(glGetUniformLocation(cubeShader, "refractionIndex"), ior);
        glUniform1f(glGetUniformLocation(cubeShader, "fresnelPower"), fresnelPwr);
        glUniform1f(glGetUniformLocation(cubeShader, "reflectionStrength"), reflectStr);
    }
}

inline void Sphere::draw(unsigned int envCubemap, const glm::mat4& model, const glm::mat4& view,
                         const glm::mat4& projection, const glm::vec3& cameraPos, bool transparent) const
{
    glUseProgram(cubeShader);

    glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(cubeShader, "cameraPos"), 1, glm::value_ptr(cameraPos));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    renderSphere();

    if (transparent) {
        glDisable(GL_BLEND);
    }
}

#endif // SPHERE_H
