#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>


class Shader {
public:
    GLuint ID = 0;

    Shader(const std::string& vertexPath, const std::string& fragmentPath);

    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;

    void use() const { glUseProgram(ID); }

    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& vec) const;
    void setVec4(const std::string& name, const glm::vec4& vec) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    std::string loadFile(const std::string& path);
    GLuint compileShader(GLenum type, const char* src);
    void checkCompileErrors(GLuint shader, const std::string& type);
    void checkLinkErrors(GLuint program);
};

#endif // SHADER_H
