#ifndef PROCEDURALSKY_H
#define PROCEDURALSKY_H

#include "Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>



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
const char *fragmentShaderSource = R"(
    #version 330 core

    in vec2 v_clipSpace;
    out vec4 FragColor;

    uniform mat4 u_inverzniProjekce;
    uniform mat4 u_inverzniPohled;
    uniform vec3 u_sunDirection;
    uniform float u_time;

    // --- ŠUMOVÉ FUNKCE PRO MRAKY (FBM - Fractal Brownian Motion) ---

    float random(vec2 st) {
        return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
    }

    float noise(vec2 st) {
        vec2 i = floor(st);
        vec2 f = fract(st);
        float a = random(i);
        float b = random(i + vec2(1.0, 0.0));
        float c = random(i + vec2(0.0, 1.0));
        float d = random(i + vec2(1.0, 1.0));
        vec2 u = f * f * (3.0 - 2.0 * f);
        return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
    }

    // FBM (6 oktáv)
    float fbm(vec2 st) {
        float value = 0.0;
        float amplitude = 0.5;
        float frequency = 1.0;
        for (int i = 0; i < 6; i++) {
            value += amplitude * noise(st * frequency);
            st *= 2.0;
            amplitude *= 0.5;
        }
        return value;
    }
    // ---------------------------------------------------------------

    void main()
    {
        // Transformace pro směr pohledu
        vec4 clip = vec4(v_clipSpace, 1.0, 1.0);
        vec4 view = u_inverzniProjekce * clip;
        view = view / view.w;
        vec4 world = u_inverzniPohled * vec4(view.xyz, 0.0);
        vec3 direction = normalize(world.xyz);

        // --- 1. Základní barva oblohy a slunce ---
        vec3 sunDir = normalize(u_sunDirection);
        float sunHeight = smoothstep(-0.1, 0.2, sunDir.y);

        vec3 dayTopColor = vec3(0.5, 0.7, 1.0);
        vec3 sunsetTopColor = vec3(0.3, 0.4, 0.6);
        vec3 dayBottomColor = vec3(0.9, 0.9, 1.0);
        vec3 sunsetBottomColor = vec3(0.9, 0.6, 0.4);

        vec3 topColor = mix(sunsetTopColor, dayTopColor, sunHeight);
        vec3 bottomColor = mix(sunsetBottomColor, dayBottomColor, sunHeight);
        float t = 0.5 * (direction.y + 1.0);
        vec3 skyColor = mix(bottomColor, topColor, t);

        vec3 sunColor = vec3(1.0, 0.9, 0.8);
        float dotSun = dot(direction, sunDir);
        float sunGlow = smoothstep(0.998, 1.0, dotSun);
        float sunDisk = smoothstep(0.9999, 1.0, dotSun);

        vec3 finalColor = skyColor + sunColor * sunGlow * 0.5 + sunColor * sunDisk;

        // -------------------------------------------------------------------
        // --- 2. Logika pro Procedurální Mraky (OPRAVENO A VYLEPŠENO) ---
        // -------------------------------------------------------------------

        // OPRAVA: Sférické mapování (Azimut)
        // Vrací hodnotu 0.0 až 1.0 pro horizontální rotaci oblohy
        float azimuth = atan(direction.x, direction.z) * 0.15915 + 0.5;

        // ZVÝŠENÍ MĚŘÍTKA A DEFINICE MRAKŮ
        vec2 cloudUV = vec2(azimuth, direction.y);

        float cloudScale = 5.0; // Původní 2.0 bylo příliš malé
        cloudUV.x *= cloudScale;
        cloudUV.y *= cloudScale * 2.0;

        // Animace
        cloudUV.x += u_time * 0.005;
        cloudUV.y += u_time * 0.002;

        float density = fbm(cloudUV);

        // OPRAVA: Volumetrický pokles hustoty (simulace tloušťky mraků)
        float cloudAltitude = 0.0; // Střed mraků je na horizontu (y=0)
        float cloudThickness = 0.5; // Zvětšení tloušťky pro viditelnost

        float heightMask = 1.0 - abs(direction.y - cloudAltitude) / cloudThickness;
        heightMask = clamp(heightMask, 0.0, 1.0);
        heightMask = pow(heightMask, 2.0); // Zostření masky

        // Finální hustota a tvar
        float finalDensity = pow(density, 2.0) * heightMask;

        // Prahování
        float cloudThreshold = 0.3;
        float cloudMask = smoothstep(cloudThreshold, cloudThreshold + 0.2, finalDensity);

        // Osvětlení a barva
        float lightDot = dot(direction, sunDir);
        float mieScatter = pow(smoothstep(-0.3, 1.0, lightDot), 4.0);

        vec3 cloudColor = mix(vec3(0.9), topColor * 1.2, sunHeight);

        // Osvětlení: Osvětlená barva od slunce (Mie rozptyl)
        vec3 illuminatedColor = cloudColor * (1.0 + mieScatter * 1.5);

        // Stínování: Tmavá spodní strana mraků
        vec3 shadowColor = vec3(0.2, 0.3, 0.4) * (1.0 - sunHeight * 0.5);

        // Tmavne směrem dolů pro simulaci stínu na dně mraků
        float darkness = pow(clamp(-direction.y, 0.0, 1.0), 3.0);
        vec3 finalCloudColor = mix(illuminatedColor, shadowColor, darkness * 0.5 + (1.0 - lightDot) * 0.2);

        // Finální mix: mraky se mísí s oblohou
        finalColor = mix(finalColor, finalCloudColor, cloudMask);

        FragColor = vec4(finalColor, 1.0);
    }
)";

class ProceduralSky {
public:

    bool Setup() {

        Shader shaderProgram1(vertexShaderSource, fragmentShaderSource, true);
        m_skyShader  = shaderProgram1.ID;

        glGenVertexArrays(1, &m_skyVAO);
        glBindVertexArray(m_skyVAO);

        glBindVertexArray(0);

        return true;
    }

    ~ProceduralSky() {
        glDeleteVertexArrays(1, &m_skyVAO);
        // glDeleteProgram(m_skyShader);
    }


    void Draw(const glm::mat4 &invView, const glm::mat4 &invProjection,
              const glm::vec3 &sunDirection, float time) {


        glDepthMask(GL_FALSE);
        glUseProgram(m_skyShader);

        glUniformMatrix4fv(glGetUniformLocation(m_skyShader, "u_inverzniProjekce"),
                           1, GL_FALSE, glm::value_ptr(invProjection));
        glUniformMatrix4fv(glGetUniformLocation(m_skyShader, "u_inverzniPohled"), 1,
                           GL_FALSE, glm::value_ptr(invView));
        glUniform3fv(glGetUniformLocation(m_skyShader, "u_sunDirection"), 1,
                     glm::value_ptr(sunDirection));

        glUniform1f(glGetUniformLocation(m_skyShader, "u_time"), time);


        glBindVertexArray(m_skyVAO);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(0);
        glDepthMask(GL_TRUE); // Obnoví zápis do Z-bufferu pro zbytek scény
    }

private:
    unsigned int m_skyShader = 0;
    unsigned int m_skyVAO = 0;
};

#endif // PROCEDURALSKY_H
