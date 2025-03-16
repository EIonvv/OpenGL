#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>

#include <spdlog/spdlog.h>


void checkShaderCompilation(GLuint shader, const std::string &type)
{
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        spdlog::error("{} shader compilation failed: {}", type, infoLog);
    }
}

void checkProgramLinking(GLuint program)
{
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        spdlog::error("Shader program linking failed: {}", infoLog);
    }
}

GLuint createShaderProgram(const std::string &vertexSource, const std::string &fragmentSource)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char *vertexSrc = vertexSource.c_str();
    glShaderSource(vertexShader, 1, &vertexSrc, NULL);
    glCompileShader(vertexShader);
    checkShaderCompilation(vertexShader, "Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char *fragmentSrc = fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
    glCompileShader(fragmentShader);
    checkShaderCompilation(fragmentShader, "Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    checkProgramLinking(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}


#endif // SHADER_H