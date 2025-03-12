#ifndef VERTEX_H
#define VERTEX_H

#include <glm/glm.hpp>

struct Vertex {
    float pos[3];
    float col[3];
    float texCoord[2];  // Added texture coordinates
};

#endif /* VERTEX_H */
