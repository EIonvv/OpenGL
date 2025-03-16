#ifndef CUBE_STRUCTURE_H
#define CUBE_STRUCTURE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Cube
{
    GLuint VAO, VBO, EBO;
    glm::vec3 position;
    glm::vec3 rotationAngles;
    // Add other cube-specific properties as needed
};

#endif /* CUBE_STRUCTURE_H */
