#include <iostream>
//#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);


const char* VS_SOURCE = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos, 1.0); // Odeslání pozice do GS
}
)glsl";

// 2. Geometry Shader (GS): Přemění bod na quad (2 trojúhelníky)
const char* GS_SOURCE = R"glsl(
#version 330 core
// Vstup: Jeden bod (point)
layout (points) in;
// Výstup: Spojitý pás trojúhelníků (triangle_strip), max. 4 vrcholy (quad)
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void buildQuad(vec4 position)
{
    // Velikost generovaného quadu v World Space
    float size = 0.5;

    // Normální vektor kamery ve World Space (potřebný pro orientaci)
    // Protože chceme, aby se quad vždy díval na kameru, použijeme vektor 'up' a 'right' z kamery
    mat4 invView = inverse(view);
    vec3 up = invView[1].xyz;  // Kamera 'nahoru'
    vec3 right = invView[0].xyz; // Kamera 'doprava'

    // Centrální pozice bodu ve World Space (získáno z VS)
    vec3 centerWorldPos = position.xyz;

    // 1. Horní pravý roh
    gl_Position = projection * view * vec4(centerWorldPos + (right + up) * size, 1.0);
    EmitVertex();

    // 2. Dolní pravý roh
    gl_Position = projection * view * vec4(centerWorldPos + (right - up) * size, 1.0);
    EmitVertex();

    // 3. Horní levý roh
    gl_Position = projection * view * vec4(centerWorldPos + (-right + up) * size, 1.0);
    EmitVertex();

    // 4. Dolní levý roh
    gl_Position = projection * view * vec4(centerWorldPos + (-right - up) * size, 1.0);
    EmitVertex();

    EndPrimitive();
}

void main()
{
    // gl_in[0].gl_Position je pozice našeho vstupního bodu
    buildQuad(gl_in[0].gl_Position);
}
)glsl";


const char* FS_SOURCE = R"glsl(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.0, 0.5, 1.0, 1.0); // Modrý billboard
}
)glsl";

unsigned int compileShader(GLenum type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, NULL, infoLog);
        std::cerr << "CHYBA::SHADER::KOMPILACE::SELDALA: " << infoLog << std::endl;
    }
    return id;
}

unsigned int createProgram(const char* vs, const char* gs, const char* fs) {
    unsigned int program = glCreateProgram();
    unsigned int vShader = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int gShader = compileShader(GL_GEOMETRY_SHADER, gs); // Vytvoření GS
    unsigned int fShader = compileShader(GL_FRAGMENT_SHADER, fs);

    glAttachShader(program, vShader);
    glAttachShader(program, gShader); // Připojení GS
    glAttachShader(program, fShader);
    glLinkProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "CHYBA::PROGRAM::LINKOVANI::SELDALA: " << infoLog << std::endl;
    }

    glDeleteShader(vShader);
    glDeleteShader(gShader);
    glDeleteShader(fShader);
    return program;
}


int main()
{

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Geometry Shader Example", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // 2. Kompilace shader programu s Geometry Shaderem
    unsigned int shaderProgram = createProgram(VS_SOURCE, GS_SOURCE, FS_SOURCE);

    // 3. Nastavení VBO a VAO pro JEDINÝ BOD
    float pointData[] = {
        0.0f, 0.0f, 0.0f // Pozice bodu uprostřed scény
    };
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pointData), pointData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);


    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);

        // 5. Použití programu a nastavení uniforms
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // 6. Vykreslení: Místo trojúhelníků vykreslujeme BODY!
        // Geometry Shader je ten, který je transformuje na trojúhelníky.
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, 1); // Pouze 1 bod

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}


void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    float cameraSpeed = 0.05f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}




