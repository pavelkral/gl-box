#ifndef PROCEDURALSKY_H
#define PROCEDURALSKY_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>

// Pro zjednodušení a odstranění závislosti na externím cpp souboru
// jsou členské funkce definovány inline (implicitně u definic v těle třídy).

class ProceduralSky {
public:

    bool Setup() {
        m_skyShader = compile_shaders(vertexShaderSource, fragmentShaderSource);

        if (m_skyShader == 0) {
            std::cerr << "Nepodarilo se zkompilovat Skydome shadery." << std::endl;
            return false;
        }

        // Vytvoření VAO. Protože geometrie je generována ve vertex shaderu pomocí
        // gl_VertexID, nepotřebujeme VBO, ale VAO je stále nutné.
        glGenVertexArrays(1, &m_skyVAO);
        glBindVertexArray(m_skyVAO);
        // Odpojení VAO
        glBindVertexArray(0);

        return true;
    }
    ~ProceduralSky() {
        glDeleteVertexArrays(1, &m_skyVAO);
       // glDeleteBuffers(1, &VBO);
       // glDeleteBuffers(1, &EBO); //  EBO!
    }
    // Vykreslí skydome. Měla by být volána s GL_FALSE pro glDepthMask.
    void Draw(const glm::mat4 &invView, const glm::mat4 &invProjection,
              const glm::vec3 &sunDirection) {
        // 1. Nastaví stav OpenGL
        glDepthMask(GL_FALSE); // Ignoruje zápis do Z-bufferu (důležité pro
        // nekonečnou oblohu)
        glUseProgram(m_skyShader);


        glUniformMatrix4fv(glGetUniformLocation(m_skyShader, "u_inverzniProjekce"),
                           1, GL_FALSE, glm::value_ptr(invProjection));
        glUniformMatrix4fv(glGetUniformLocation(m_skyShader, "u_inverzniPohled"), 1,
                           GL_FALSE, glm::value_ptr(invView));
        glUniform3fv(glGetUniformLocation(m_skyShader, "u_sunDirection"), 1,
                     glm::value_ptr(sunDirection));

        glBindVertexArray(m_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 4. Obnoví stav OpenGL
        glBindVertexArray(0);
        glDepthMask(GL_TRUE); // Obnoví zápis do Z-bufferu pro zbytek scény
    }

private:
    unsigned int m_skyShader = 0;
    unsigned int m_skyVAO = 0;

    // Zdrojový kód vertex shaderu
    const char *vertexShaderSource = R"(
    #version 330 core
    out vec2 v_clipSpace;

    void main()
    {
        // Vytváří trojúhelník pokrývající celou obrazovku pomocí gl_VertexID
        float x = -1.0 + float((gl_VertexID & 1) << 2);
        float y = -1.0 + float((gl_VertexID & 2) << 1);

        v_clipSpace = vec2(x, y);
        gl_Position = vec4(v_clipSpace, 1.0, 1.0);
    }
    )";

    // Zdrojový kód fragment shaderu
    const char *fragmentShaderSource = R"(
    #version 330 core

    in vec2 v_clipSpace;
    out vec4 FragColor;

    uniform mat4 u_inverzniProjekce;
    uniform mat4 u_inverzniPohled;
    uniform vec3 u_sunDirection;

    void main()
    {
        // Transformace z clip-space do world-space pro získání směru pohledu
        vec4 clip = vec4(v_clipSpace, 1.0, 1.0);
        vec4 view = u_inverzniProjekce * clip;
        view = view / view.w; // Perspektivní korekce

        // Transformace do world-space (w=0.0 pro směr)
        vec4 world = u_inverzniPohled * vec4(view.xyz, 0.0);
        vec3 direction = normalize(world.xyz);

        // --- LOGIKA PRO OBLOHU A SLUNCE ---

        // 1. Základní barva oblohy (Sky Gradient)
        float sunHeight = smoothstep(-0.1, 0.2, normalize(u_sunDirection).y);

        // Denní vs. Západní/Východní barvy
        vec3 dayTopColor = vec3(0.5, 0.7, 1.0);
        vec3 sunsetTopColor = vec3(0.3, 0.4, 0.6);
        vec3 dayBottomColor = vec3(0.9, 0.9, 1.0);
        vec3 sunsetBottomColor = vec3(0.9, 0.6, 0.4);

        vec3 topColor = mix(sunsetTopColor, dayTopColor, sunHeight);
        vec3 bottomColor = mix(sunsetBottomColor, dayBottomColor, sunHeight);

        // Gradient shora dolů
        float t = 0.5 * (direction.y + 1.0);
        vec3 skyColor = mix(bottomColor, topColor, t);

        // 2. Vykreslení slunce (Glow a Disk)
        vec3 sunColor = vec3(1.0, 0.9, 0.8);
        float dotSun = dot(direction, normalize(u_sunDirection));

        // Měkká záře
        float sunGlow = smoothstep(0.998, 1.0, dotSun);

        // Ostrý disk
        float sunDisk = smoothstep(0.9999, 1.0, dotSun);

        // 3. Kombinace barev
        vec3 finalColor = skyColor + sunColor * sunGlow * 0.5 + sunColor * sunDisk;

        FragColor = vec4(finalColor, 1.0);
    }
    )";


    unsigned int compile_shaders(const char *vertSource, const char *fragSource) {
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertSource, NULL);
        glCompileShader(vertexShader);

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragSource, NULL);
        glCompileShader(fragmentShader);

        unsigned int shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);


        int success;
        char infoLog[512];
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cerr << "CHYBA::SKYDOMESHADER::LINKOVANI_SE_NEZDARILO\n"
                      << infoLog << std::endl;
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return shaderProgram;
    }
};
#endif // PROCEDURALSKY_H
