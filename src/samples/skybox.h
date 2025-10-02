#ifndef SKYBOX_H
#define SKYBOX_H
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>


// --- Prototypy funkcí ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int loadCubemap(std::vector<std::string> faces);

// --- Nastavení ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Zdrojové kódy shaderů (zůstávají stejné) ---

// Vertex shader pro skybox
const char *skyboxVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    out vec3 TexCoords;

    uniform mat4 projection;
    uniform mat4 view;

    void main()
    {
        TexCoords = aPos;
        // Odebereme translaci z view matice, aby se skybox nepohyboval s kamerou
        vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
        gl_Position = pos.xyww; // Zajišťuje, že hloubka je vždy 1.0 (nejdále)
    }
)";

// Fragment shader pro skybox
const char *skyboxFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec3 TexCoords;

    uniform samplerCube skybox;

    void main()
    {
        FragColor = texture(skybox, TexCoords);
    }
)";

// Vertex shader pro slunce (jednoduchý objekt)
const char *sunVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

// Fragment shader pro slunce (jednoduchá barva)
const char *sunFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(1.0, 1.0, 0.8, 1.0); // Jasně žlutá barva slunce
    }
)";


int main()
{
    // --- Inicializace GLFW ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // --- Vytvoření okna ---
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Skybox se Sluncem (s GLM)", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // --- Inicializace GLAD ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // --- Konfigurace OpenGL ---
    glEnable(GL_DEPTH_TEST);
    // Nastavení hloubkové funkce pro skybox optimalizaci
    glDepthFunc(GL_LEQUAL);


    // --- Kompilace shaderů (stejný kód jako předtím) ---

    // Skybox shader
    unsigned int skyboxVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(skyboxVertexShader, 1, &skyboxVertexShaderSource, NULL);
    glCompileShader(skyboxVertexShader);
    // ... kontrola chyb (pro zkrácení vynechána) ...

    unsigned int skyboxFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(skyboxFragmentShader, 1, &skyboxFragmentShaderSource, NULL);
    glCompileShader(skyboxFragmentShader);
    // ... kontrola chyb ...

    unsigned int skyboxShaderProgram = glCreateProgram();
    glAttachShader(skyboxShaderProgram, skyboxVertexShader);
    glAttachShader(skyboxShaderProgram, skyboxFragmentShader);
    glLinkProgram(skyboxShaderProgram);
    // ... kontrola chyb ...
    glDeleteShader(skyboxVertexShader);
    glDeleteShader(skyboxFragmentShader);

    // Sun shader
    unsigned int sunVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(sunVertexShader, 1, &sunVertexShaderSource, NULL);
    glCompileShader(sunVertexShader);

    unsigned int sunFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sunFragmentShader, 1, &sunFragmentShaderSource, NULL);
    glCompileShader(sunFragmentShader);

    unsigned int sunShaderProgram = glCreateProgram();
    glAttachShader(sunShaderProgram, sunVertexShader);
    glAttachShader(sunShaderProgram, sunFragmentShader);
    glLinkProgram(sunShaderProgram);
    glDeleteShader(sunVertexShader);
    glDeleteShader(sunFragmentShader);


    // --- Data pro vrcholy (stejná jako předtím) ---
    float skyboxVertices[] = {
        // pozice
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };


    // --- Vytvoření VAO a VBO pro skybox ---
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // --- Vytvoření VAO a VBO pro slunce ---
    unsigned int sunVAO, sunVBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    // Můžeme znovu použít data skyboxu
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


    // --- Načtení textury cubemapy ---
    std::vector<std::string> faces
        {
            "skybox/right.jpg",
            "skybox/left.jpg",
            "skybox/top.jpg",
            "skybox/bottom.jpg",
            "skybox/front.jpg",
            "skybox/back.jpg"
        };

    std::vector<std::string> faces1
        {
            "skybox2/right.bmp",
            "skybox2/left.bmp",
            "skybox2/top.bmp",
            "skybox2/bottom.bmp",
            "skybox2/front.bmp",
            "skybox2/back.bmp"
        };
    unsigned int cubemapTexture = loadCubemap(faces1);

    // --- Nastavení shaderů před hlavní smyčkou ---
    glUseProgram(skyboxShaderProgram);
    glUniform1i(glGetUniformLocation(skyboxShaderProgram, "skybox"), 0);


    // --- Hlavní smyčka (Render loop) ---
    while (!glfwWindowShouldClose(window))
    {
        // --- Zpracování vstupu ---
        processInput(window);

        // --- Vykreslování ---
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- GLM MATICE ---
        // Vytvoření matic pomocí GLM
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);

        // Projekční matice (perspektiva)
        projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // View matice (kamera)
        // Jednoduchá rotace kamery v čase pro ukázku
        float radius = 5.0f;
        float camX = sin(glfwGetTime() * 0.5f) * radius;
        float camZ = cos(glfwGetTime() * 0.5f) * radius;
        view = glm::lookAt(glm::vec3(camX, 0.0f, camZ), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));


        // --- Vykreslení slunce ---
        glUseProgram(sunShaderProgram);

        // Model matice pro slunce - posune a zmenší krychli
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(10.0f, 5.0f, -20.0f)); // Pozice slunce
        model = glm::scale(model, glm::vec3(0.2f)); // Velikost slunce

        // Předání matic do shaderu slunce
        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(sunVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);


        // --- Vykreslení skyboxu (jako poslední) ---
        glDepthFunc(GL_LEQUAL); // Změna hloubkové funkce
        glUseProgram(skyboxShaderProgram);

        // Odebereme translaci z view matice pro skybox. GLM to umí elegantně.
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // ... vykreslení skyboxu ...
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS); // Vrátíme hloubkovou funkci na výchozí hodnotu

        // --- Výměna bufferů a zpracování událostí ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Uvolnění zdrojů ---
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteProgram(skyboxShaderProgram);
    glDeleteProgram(sunShaderProgram);

    glfwTerminate();
    return 0;
}


// --- Implementace funkcí (beze změny) ---

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Funkce pro načtení cubemapy
unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false); // Cubemapy obvykle nemají převrácenou Y osu
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}





#endif // SKYBOX_H
