#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <spdlog/spdlog.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <GLFW/glfw3.h>

class TextRenderer
{
public:
    struct Character
    {
        GLuint textureID;
        glm::ivec2 size;
        glm::ivec2 bearing;
        GLuint advance;
    };

    TextRenderer(const char *fontPath, GLuint fontSize);
    ~TextRenderer();
    void renderText(std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color);

private:
    std::map<GLchar, Character> characters;
    GLuint VAO, VBO;
    GLuint shaderProgram;

    void initializeShader();
};

#endif /* TEXT_RENDERER_H */
