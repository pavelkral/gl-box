#ifndef TESSELATION_H
#define TESSELATION_H
#include <iostream>
#include <vector>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// 1. Vertex Shader (VS) - Pouze předává pozici dál do TCS
const char *vertexShaderSource = R"glsl(
#version 460 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos, 1.0);
}
)glsl";

// 2. Tessellation Control Shader (TCS) - Definuje úroveň dělení
const char *tessControlShaderSource = R"glsl(
#version 460 core
// Vstup (4 vrcholy) a výstup (také 4 vrcholy - žádné nepřidáváme, jen řídíme)
layout (vertices = 4) out;

uniform float tessLevel;

void main()
{
    // Nastavení úrovní tesační.
    // Zde nastavujeme všechny vnější a vnitřní úrovně na stejnou hodnotu 'tessLevel'
    gl_TessLevelOuter[0] = tessLevel;
    gl_TessLevelOuter[1] = tessLevel;
    gl_TessLevelOuter[2] = tessLevel;
    gl_TessLevelOuter[3] = tessLevel;
    gl_TessLevelInner[0] = tessLevel;
    gl_TessLevelInner[1] = tessLevel;

    // Předání pozice do Evaluation Shaderu
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
)glsl";

// 3. Tessellation Evaluation Shader (TES) - Vytváří novou geometrii
const char *tessEvaluationShaderSource = R"glsl(
#version 460 core
// quads = Tesační generátor vytvoří 4-stranné patche (kvádry)
// equal_spacing = Rovnoměrné rozložení vrcholů
layout (quads, equal_spacing) in;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float amplitude;

void main()
{
    // gl_TessCoord je 3D vektor s barycentrickými souřadnicemi (u, v) pro kvádr.

    // Lineární interpolace (u, v) pro získání pozice v 3D prostoru
    vec4 p0 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 p1 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
    vec4 p_interp = mix(p0, p1, gl_TessCoord.y);

    // Přidání jednoduché deformace na základě pozice (simulace terénu/vln)
    float offset = amplitude * sin(p_interp.x * 3.0 + p_interp.z * 3.0);

    // Aplikace transformací
    gl_Position = projection * view * model * vec4(p_interp.x, offset, p_interp.z, 1.0);
}
)glsl";

// 4. Fragment Shader (FS) - Jednoduché barvení
const char *fragmentShaderSource = R"glsl(
#version 460 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.1, 0.5, 0.2, 1.0); // Tmavě zelená barva
}
)glsl";

// --- GLOBAL VARIABLES ---
GLFWwindow* window;
const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

// --- FUNKCE ---

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Kompilace a linkování shaderů
GLuint compileShaders()
{
    // Vytvoření programu a shaderů
    GLuint program = glCreateProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    glAttachShader(program, vs);

    GLuint tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
    glShaderSource(tcs, 1, &tessControlShaderSource, NULL);
    glCompileShader(tcs);
    glAttachShader(program, tcs);

    GLuint tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
    glShaderSource(tes, 1, &tessEvaluationShaderSource, NULL);
    glCompileShader(tes);
    glAttachShader(program, tes);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    glAttachShader(program, fs);

    // Linkování
    glLinkProgram(program);

    // (Zde by měla být kontrola chyb kompilace a linkování!)

    // Smazání již připojených shaderů
    glDeleteShader(vs);
    glDeleteShader(tcs);
    glDeleteShader(tes);
    glDeleteShader(fs);

    return program;
}


int main()
{

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6); // 4.6 , min. 4.0
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL Tessellation Example", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 3. Povolení Tessellation
    // Nastavení, že každý vykreslovací příkaz bude zpracován jako 4-vrcholový patch
    glPatchParameteri(GL_PATCH_VERTICES, 4);

    GLuint shaderProgram = compileShaders();

    // 5. Příprava dat (VAO a VBO)
    // Vytvoříme jednoduchý kvádr o 4 rozích
    float vertices[] = {
        // Pozice (x, y, z)
        -1.0f, 0.0f, -1.0f, // Dolní levý
        1.0f, 0.0f, -1.0f, // Dolní pravý
        1.0f, 0.0f,  1.0f, // Horní pravý
        -1.0f, 0.0f,  1.0f  // Horní levý
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // DSA styl (glCreateBuffers/glNamedBufferData) by zde mohl být použit,
    // ale pro VAO/VBO často zůstává bind-to-modify, jelikož je to pro 4.x stále podporováno a srozumitelnější pro začátek.

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Nastavení vertex atributu (pozice)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // --- GLM & Uniforms ---

        float time = (float)glfwGetTime();

        // 1. Model Matrix (jednoduchá rotace pro lepší vizualizaci)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, time * 0.1f, glm::vec3(0.0f, 1.0f, 0.0f));

        // 2. View Matrix (kamera)
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0.0f, -0.5f, -3.0f));

        // 3. Projection Matrix
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // 4. Odeslání Uniforms do TES
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Dynamické nastavení úrovně tesační (mění se s časem)
        float tessLevel = 1.0f + 10.0f * (sin(time) * 0.5f + 0.5f); // Úroveň se mění mezi 1.0 a 11.0
        glUniform1f(glGetUniformLocation(shaderProgram, "tessLevel"), tessLevel);

        // Dynamické nastavení amplitudy deformace
        float amplitude = 0.5f * (sin(time * 0.5f) * 0.5f + 0.5f); // Amplituda vln se mění
        glUniform1f(glGetUniformLocation(shaderProgram, "amplitude"), amplitude);

        // 7. Vykreslení!
        glBindVertexArray(VAO);
        // Místo glDrawArrays(GL_TRIANGLES, ...) použijeme GL_PATCHES
       // glDrawArrays(GL_PATCHES, 0, 4);

        // Vykreslování drátěného modelu pro lepší vizualizaci tesační (volitelné)
         glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
         glDrawArrays(GL_PATCHES, 0, 4);
         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
#endif // TESSELATION_H
