#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H

#include "../../callbacks/keyboard/keyboard.h"
#include "../../callbacks/mouse/mouse_callback.h"
#include "../../mouse/mouse_position/get_mouse_position.h"
#include "../setupRenderer.h"
#include "../imGui/debug_imgui.h"
#include "structures/plane/plane_struct.h"

// Cube data
static const Vertex cubeVertices[8] = {
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // 0: Front bottom-left
    {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},   // 1: Front bottom-right
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},    // 2: Front top-right
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // 3: Front top-left
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 4: Back bottom-left
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},  // 5: Back bottom-right
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},   // 6: Back top-right
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // 7: Back top-left
};

static const GLuint cubeIndices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
    5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
    3, 2, 6, 6, 7, 3, 0, 4, 5, 5, 1, 0};

// Mouse-cube intersection
bool isPointInCube(glm::vec2 mousePos, const glm::mat4 &mvp, int width, int height)
{

    glm::vec4 screenVertices[8];
    for (int i = 0; i < 8; i++)
    {
        glm::vec4 vertex(cubeVertices[i].pos[0], cubeVertices[i].pos[1], cubeVertices[i].pos[2], 1.0f);
        screenVertices[i] = mvp * vertex;
        screenVertices[i] /= screenVertices[i].w;
        screenVertices[i].x = (screenVertices[i].x + 1.0f) * width * 0.5f;
        screenVertices[i].y = (1.0f - screenVertices[i].y) * height * 0.5f;
    }

    auto pointInTriangle = [](glm::vec2 p, glm::vec2 v1, glm::vec2 v2, glm::vec2 v3)
    {
        float d1 = (p.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (p.y - v2.y);
        float d2 = (p.x - v3.x) * (v2.y - v3.y) - (v2.x - v3.x) * (p.y - v3.y);
        float d3 = (p.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (p.y - v1.y);
        bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(has_neg && has_pos);
    };

    glm::vec2 screenVertices2D[8];
    for (int i = 0; i < 8; i++)
    {
        screenVertices2D[i] = glm::vec2(screenVertices[i].x, screenVertices[i].y);
    }

    return pointInTriangle(mousePos, screenVertices2D[0], screenVertices2D[1], screenVertices2D[2]) ||
           pointInTriangle(mousePos, screenVertices2D[2], screenVertices2D[3], screenVertices2D[0]) ||
           pointInTriangle(mousePos, screenVertices2D[1], screenVertices2D[5], screenVertices2D[6]) ||
           pointInTriangle(mousePos, screenVertices2D[6], screenVertices2D[2], screenVertices2D[1]) ||
           pointInTriangle(mousePos, screenVertices2D[5], screenVertices2D[4], screenVertices2D[7]) ||
           pointInTriangle(mousePos, screenVertices2D[7], screenVertices2D[6], screenVertices2D[5]) ||
           pointInTriangle(mousePos, screenVertices2D[4], screenVertices2D[0], screenVertices2D[3]) ||
           pointInTriangle(mousePos, screenVertices2D[3], screenVertices2D[7], screenVertices2D[4]) ||
           pointInTriangle(mousePos, screenVertices2D[3], screenVertices2D[2], screenVertices2D[6]) ||
           pointInTriangle(mousePos, screenVertices2D[6], screenVertices2D[7], screenVertices2D[3]) ||
           pointInTriangle(mousePos, screenVertices2D[0], screenVertices2D[4], screenVertices2D[5]) ||
           pointInTriangle(mousePos, screenVertices2D[5], screenVertices2D[1], screenVertices2D[0]);
}

// Cube-plane collision
bool isCubeCollidingWithPlane(const glm::mat4 &model, const std::vector<Plane> &planes, int &collidingPlaneIndex)
{

    auto checkCollisionWithPlane = [&](const Plane &plane) -> bool
    {
        std::array<glm::vec3, 8> cubeWorldVertices;
        for (int i = 0; i < 8; i++)
        {
            glm::vec4 vertex(cubeVertices[i].pos[0], cubeVertices[i].pos[1], cubeVertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = model * vertex;
            cubeWorldVertices[i] = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
        }

        std::array<glm::vec3, 4> planeWorldVertices;
        glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), plane.position);
        for (int i = 0; i < 4; i++)
        {
            glm::vec4 vertex(plane.vertices[i].pos[0], plane.vertices[i].pos[1], plane.vertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = planeModel * vertex;
            planeWorldVertices[i] = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
        }

        glm::vec3 edge1 = planeWorldVertices[1] - planeWorldVertices[0];
        glm::vec3 edge2 = planeWorldVertices[2] - planeWorldVertices[0];
        glm::vec3 planeNormal = glm::normalize(glm::cross(edge1, edge2));

        std::array<glm::vec3, 3> cubeNormals = {
            glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
        glm::mat3 normalMatrix = glm::mat3(model);
        for (auto &normal : cubeNormals)
        {
            normal = glm::normalize(normalMatrix * normal);
        }

        std::array<glm::vec3, 4> testAxes = {planeNormal, cubeNormals[0], cubeNormals[1], cubeNormals[2]};

        for (const auto &axis : testAxes)
        {
            float cubeMin = std::numeric_limits<float>::max();
            float cubeMax = std::numeric_limits<float>::lowest();
            for (const auto &vertex : cubeWorldVertices)
            {
                float proj = glm::dot(vertex, axis);
                cubeMin = std::min(cubeMin, proj);
                cubeMax = std::max(cubeMax, proj);
            }

            float planeMin = std::numeric_limits<float>::max();
            float planeMax = std::numeric_limits<float>::lowest();
            for (const auto &vertex : planeWorldVertices)
            {
                float proj = glm::dot(vertex, axis);
                planeMin = std::min(planeMin, proj);
                planeMax = std::max(planeMax, proj);
            }

            if (cubeMax < planeMin || planeMax < cubeMin)
            {
                return false;
            }
        }

        glm::vec3 planeEdge1 = planeWorldVertices[1] - planeWorldVertices[0];
        glm::vec3 planeEdge2 = planeWorldVertices[3] - planeWorldVertices[0];
        for (const auto &vertex : cubeWorldVertices)
        {
            glm::vec3 toVertex = vertex - planeWorldVertices[0];
            float u = glm::dot(toVertex, planeEdge1) / glm::dot(planeEdge1, planeEdge1);
            float v = glm::dot(toVertex, planeEdge2) / glm::dot(planeEdge2, planeEdge2);

            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
            {
                float distance = glm::dot(toVertex, planeNormal);
                if (std::abs(distance) <= 0.5f)
                {
                    return true;
                }
            }
        }
        return false;
    };
    static int lastCollidingPlaneIndex = 0;
    for (size_t i = 0; i < planes.size(); ++i)
    {
        if (checkCollisionWithPlane(planes[i]))
        {
            collidingPlaneIndex = static_cast<int>(i);
            lastCollidingPlaneIndex = collidingPlaneIndex + 1;
            return true;
        }

        // if on the last one and first one then increment the lastCollidingPlaneIndex
        if (i == planes.size() - 1 && lastCollidingPlaneIndex == planes.size())
        {
            lastCollidingPlaneIndex += 1;
            collidingPlaneIndex = lastCollidingPlaneIndex;
        }
    }
    collidingPlaneIndex = lastCollidingPlaneIndex;
    return false;
}

// Render scene
void renderScene(GLFWwindow *window, GLuint program, GLint mvp_location, GLuint cubeVAO, GLuint cubeEBO, const std::vector<Plane> &planes, const glm::mat4 &model, float ratio)
{

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x));
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + direction, up);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);

    glUseProgram(program);
    GLint useTextureLocation = glGetUniformLocation(program, "useTexture");

    // Render planes
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planeTexture);
    glUniform1i(textureLocation, 0);
    glUniform1i(useTextureLocation, 1);
    for (const auto &plane : planes)
    {
        glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), plane.position);
        glm::mat4 planeMVP = projection * view * planeModel;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(planeMVP));
        glBindVertexArray(plane.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane.ebo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Render cube
    if (!cubePOVMode)
    {
        glm::mat4 cubeMVP = projection * view * model;
        glUniform1i(useTextureLocation, 1);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cubeMVP));
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    // Render text
    glDisable(GL_DEPTH_TEST);
    int collidingPlaneIndex;
    isColliding = isCubeCollidingWithPlane(model, planes, collidingPlaneIndex);
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    glm::mat4 cubeMVP = projection * view * model;
    bool isMouseOverCube = isPointInCube(mousePos, cubeMVP, width, height);

    if (isMouseOverCube && !cubePOVMode)
    {
        char cursorText[64];
        snprintf(cursorText, sizeof(cursorText), "Cursor position: (%.2f, %.2f)", mousePos.x, mousePos.y);
        textRenderer->renderText(cursorText, 10.0f, 50.0f, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (!mouseInputEnabled)
    {
        // display text letting the user know about F2 in center of screen
        textRenderer->renderText("Press F2 to enable mouse input", static_cast<float>(width) / 2.0f - 200.0f, static_cast<float>(height) / 2.0f, 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glEnable(GL_DEPTH_TEST);
}

#endif /* RENDER_SCENE_H */
