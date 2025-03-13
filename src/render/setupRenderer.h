#ifndef SETUPRENDERER_H
#define SETUPRENDERER_H

#include <glad/glad.h>
#include <GL/gl.h>
#include <spdlog/spdlog.h>
#include <string>
#include <fstream>
#include <base64/base64.h>
#include "vertex/vertex.h"
#include "texture/texture.h"
#include "../config.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

static GLuint planeTexture; // Plane texture
static GLuint cubeTexture;  // Cube texture
static GLint textureLocation;

// Cube data with updated texture coordinates
static const Vertex vertices[8] = {
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // 0: Front bottom-left (Bottom face: bottom-left)
    {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},   // 1: Front bottom-right (Bottom face: bottom-right)
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},    // 2: Front top-right (Top face: top-right)
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // 3: Front top-left (Top face: top-left)
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 4: Back bottom-left (Bottom face: top-left)
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},  // 5: Back bottom-right (Bottom face: top-right)
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},   // 6: Back top-right (Top face: bottom-right)
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // 7: Back top-left (Top face: bottom-left)
};

static const GLuint indices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
    5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
    3, 2, 6, 6, 7, 3, 0, 4, 5, 5, 1, 0};

// Plane data (increased size to 12x12)
static const Vertex planeVertices[6] = {
    {{-5.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
    {{5.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
    {{5.0f, 0.0f, 5.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
    {{-5.0f, 0.0f, 5.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
    {{-5.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
    {{5.0f, 0.0f, 5.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}}};

static const GLuint planeIndices[6] = {0, 1, 2, 2, 3, 0};

// Shaders (modified fragment shader to use a fallback color if texture fails)
static const char *vertex_shader_text =
    "#version 330\n"
    "uniform mat4 MVP;\n"
    "in vec3 vPos;\n"
    "in vec3 vCol;\n"
    "in vec2 vTexCoord;\n"
    "out vec3 color;\n"
    "out vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = MVP * vec4(vPos, 1.0);\n"
    "    color = vCol;\n"
    "    texCoord = vTexCoord;\n"
    "}\n";

static const char *fragment_shader_text =
    "#version 330\n"
    "in vec3 color;\n"
    "in vec2 texCoord;\n"
    "out vec4 fragment;\n"
    "uniform sampler2D textureSampler;\n"
    "uniform int useTexture;\n"
    "void main()\n"
    "{\n"
    "    if (useTexture == 1) {\n"
    "        vec4 texColor = texture(textureSampler, texCoord);\n"
    "        if (texColor.a < 0.1) {\n"
    "            fragment = vec4(0.5, 0.5, 0.5, 1.0); // Fallback color (gray)\n"
    "        } else {\n"
    "            fragment = texColor;\n"
    "        }\n"
    "    } else {\n"
    "        fragment = vec4(color, 1.0);\n"
    "    }\n"
    "}\n";

// Setup OpenGL buffers and shaders
static void setupRendering(GLuint &program, GLint &mvp_location, GLint &vpos_location, GLint &vcol_location,
                    GLuint &vertex_array, GLuint &vertex_buffer, GLuint &element_buffer,
                    GLuint &planeVertexArray, GLuint &planeVertexBuffer, GLuint &planeElementBuffer)
{
    ZoneScoped; // Tracy: Profile this function
    // Cube setup
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &element_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Plane setup
    glGenBuffers(1, &planeVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, planeVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &planeElementBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeElementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);

    // Shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
        spdlog::error("Vertex shader compilation failed: {}", infoLog);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, infoLog);
        spdlog::error("Fragment shader compilation failed: {}", infoLog);
    }

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        spdlog::error("Shader program linking failed: {}", infoLog);
    }

    mvp_location = glGetUniformLocation(program, "MVP");
    vpos_location = glGetAttribLocation(program, "vPos");
    vcol_location = glGetAttribLocation(program, "vCol");
    GLint vtex_location = glGetAttribLocation(program, "vTexCoord");
    GLint textureLocation = glGetUniformLocation(program, "textureSampler");
    GLint useTextureLocation = glGetUniformLocation(program, "useTexture");

    // Cube VAO
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    // Enable texture coordinates for the cube
    glEnableVertexAttribArray(vtex_location);
    glVertexAttribPointer(vtex_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));

    // Plane VAO
    glGenVertexArrays(1, &planeVertexArray);
    glBindVertexArray(planeVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, planeVertexBuffer);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    glEnableVertexAttribArray(vtex_location);
    glVertexAttribPointer(vtex_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));

    // Load texture
    std::map<std::string, std::string> textureMap = {
        {"plane", "resources\\textures\\wood.jpg"},
        {"cube", "resources\\textures\\concrete.jpg"}};

    for (const auto &[textureName, texturePath] : textureMap)
    {
        if (textureName == "plane")
        {
            loadTexture(texturePath.c_str(), planeTexture);
        }
        else if (textureName == "cube")
        {
            loadTexture(texturePath.c_str(), cubeTexture);
        }
    }

    // Cleanup
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glUseProgram(program);
    glUniform1i(useTextureLocation, 1);
    glUniform1i(textureLocation, 0);
}

// Initialize text renderer
static void initializeTextRenderer(TextRenderer *&textRenderer)
{
    ZoneScoped; // Tracy: Profile this function
    try
    {
        std::string fontPath = "resources\\fonts\\arlrbd.ttf";
        spdlog::info("Font path: {}", fontPath);

        textRenderer = new TextRenderer(fontPath.c_str(), 32);
        keyboardTextRenderer = textRenderer;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to initialize TextRenderer: {}", e.what());
        throw;
    }
}

#endif /* SETUPRENDERER_H */
