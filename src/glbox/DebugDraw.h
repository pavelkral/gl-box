#ifndef DEBUGDRAW_H
#define DEBUGDRAW_H


#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <iostream>


const char* debugVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
}
)";

const char* debugFragmentShaderSource = R"(
#version 330 core
uniform vec3 color;
out vec4 FragColor;
void main() {
    FragColor = vec4(color, 1.0);
}
)";

class DebugDraw {
private:
    unsigned int shaderProgram;
    unsigned int VBO, VAO;

    void checkShaderCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "CHYBA::SHADER_KOMPILACE_CHYBA typu: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "CHYBA::PROGRAM_LINKOVANI_CHYBA typu: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }

public:
    DebugDraw() {
        // Kompilace a Linkování
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &debugVertexShaderSource, NULL);
        glCompileShader(vertex);
        checkShaderCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &debugFragmentShaderSource, NULL);
        glCompileShader(fragment);
        checkShaderCompileErrors(fragment, "FRAGMENT");

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertex);
        glAttachShader(shaderProgram, fragment);
        glLinkProgram(shaderProgram);
        checkShaderCompileErrors(shaderProgram, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        // Nastavení VAO/VBO pro dynamické kreslení čar
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Alokace místa (budeme posílat 3 floaty (x,y,z) na vrchol)
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 2, NULL, GL_DYNAMIC_DRAW);

        // Atribut 0 (pozice): 3 floaty, bez normalizace, krok 3 * sizeof(float), offset 0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~DebugDraw() {
        glDeleteProgram(shaderProgram);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }


    void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color, const glm::mat4& view, const glm::mat4& projection) {
        glUseProgram(shaderProgram);


        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(color));

        // Nahraj data čáry (jen 2 body)
        glm::vec3 vertices[] = { start, end };
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        // Zvýšíme tloušťku čáry pro lepší viditelnost (default je často jen 1.0)
        glLineWidth(2.0f);

        // Nastav GL stav (musíme zakázat GL_DEPTH_TEST, aby byla čára vidět vždy)
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_LINES, 0, 2);
        glEnable(GL_DEPTH_TEST);

        // Obnovení výchozí tloušťky
        glLineWidth(1.0f);

        glBindVertexArray(0);
    }
};
#endif // DEBUGDRAW_H
