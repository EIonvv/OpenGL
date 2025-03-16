#ifndef PLANE_STRUCT_H
#define PLANE_STRUCT_H
#include <vector>
#include <gl/GL.h>
#include "../vertex/vertex.h"

// Plane structure
struct Plane
{
    std::vector<Vertex> vertices; // Dynamic vertices
    GLuint vao, vbo, ebo;         // OpenGL buffers
    glm::vec3 position;           // Center position of the plane
};

#endif /* PLANE_STRUCT_H */
