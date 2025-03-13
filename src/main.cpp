#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/msvc_sink.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <base64/base64.h>
#include <map>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include "callbacks/keyboard/keyboard.h"
#include "callbacks/mouse/mouse_callback.h"
#include "mouse/mouse_position/get_mouse_position.h"
#include "render/vertex/vertex.h"
#include "render/text/text_renderer.h"
#include "render/texture/texture.h"
#include "config.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#include "render/setupRenderer.h"

// Global variables
static glm::vec3 SquarePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer = nullptr;
static glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
static bool cubePOVMode = false;

// Plane structure
struct Plane
{
    std::vector<Vertex> vertices; // Dynamic vertices
    GLuint vao, vbo, ebo;         // OpenGL buffers
    glm::vec3 position;           // Center position of the plane
};

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

// Error callback
static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// Generate plane vertices
std::vector<Vertex> generatePlaneVertices(float width, float depth, const glm::vec3 &position)
{
    std::vector<Vertex> vertices(4);
    float halfWidth = width / 2.0f;
    float halfDepth = depth / 2.0f;

    vertices[0] = {{position.x - halfWidth, position.y, position.z - halfDepth}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}}; // Bottom-left
    vertices[1] = {{position.x + halfWidth, position.y, position.z - halfDepth}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}}; // Bottom-right
    vertices[2] = {{position.x + halfWidth, position.y, position.z + halfDepth}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}}; // Top-right
    vertices[3] = {{position.x - halfWidth, position.y, position.z + halfDepth}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}}; // Top-left

    return vertices;
}

// Setup plane buffers
void setupPlaneBuffers(Plane &plane, GLint vpos_location, GLint vcol_location, GLint vtex_location)
{
    glGenVertexArrays(1, &plane.vao);
    glBindVertexArray(plane.vao);

    glGenBuffers(1, &plane.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, plane.vbo);
    glBufferData(GL_ARRAY_BUFFER, plane.vertices.size() * sizeof(Vertex), plane.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &plane.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    glEnableVertexAttribArray(vtex_location);
    glVertexAttribPointer(vtex_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));
}

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

    collidingPlaneIndex = -1;
    for (size_t i = 0; i < planes.size(); ++i)
    {
        if (checkCollisionWithPlane(planes[i]))
        {
            collidingPlaneIndex = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

// Initialize GLFW and OpenGL
GLFWwindow *initializeWindow()
{
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "3D Draggable Cube", NULL, NULL);
    if (!window)
    {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetWindowSizeLimits(window, 320, 240, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize GLAD");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    return window;
}

// Simple gravity function
void SimpleGravity(float deltaTime, glm::mat4 &model, const std::vector<Plane> &planes)
{
    SquarePos.y -= 0.1f * deltaTime;
    int collidingPlaneIndex;
    if (isCubeCollidingWithPlane(model, planes, collidingPlaneIndex))
    {
        float lowestY = std::numeric_limits<float>::max();
        for (int i = 0; i < 8; i++)
        {
            glm::vec4 vertex(cubeVertices[i].pos[0], cubeVertices[i].pos[1], cubeVertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = model * vertex;
            lowestY = std::min(lowestY, worldPos.y);
        }
        float planeY = planes[collidingPlaneIndex].position.y;
        if (lowestY < planeY)
            SquarePos.y += (planeY - lowestY);
    }
}

// Update cube position and orientation
void updateCube(GLFWwindow *window, glm::mat4 &model, glm::mat4 &mvp, int width, int height, float ratio, float deltaTime, const std::vector<Plane> &planes)
{
    float baseSpeed = deltaTime * 12.0f;
    float speed = baseSpeed;

    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x));
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);

    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (cubePOVMode)
    {
        speed = deltaTime * 3.0f;
        SquarePos.y = -0.5f;
        cameraPos = SquarePos + glm::vec3(0.0f, 0.5f, 0.0f);
    }

    if (pressing_w)
    {
        if (cubePOVMode)
            SquarePos += direction * speed * deltaTime * 60.0f;
        else
            cameraPos += direction * speed;
    }
    if (pressing_s)
    {
        if (cubePOVMode)
            SquarePos -= direction * speed * deltaTime * 60.0f;
        else
            cameraPos -= direction * speed;
    }
    if (pressing_a)
    {
        if (cubePOVMode)
            SquarePos -= right * speed * deltaTime * 60.0f;
        else
            cameraPos -= right * speed;
    }
    if (pressing_d)
    {
        if (cubePOVMode)
            SquarePos += right * speed * deltaTime * 60.0f;
        else
            cameraPos += right * speed;
    }

    float rotationSpeed = 1.0f;
    rotationAngles.y += static_cast<float>(mouseDelta.x) * rotationSpeed * deltaTime;
    rotationAngles.x -= static_cast<float>(mouseDelta.y) * rotationSpeed * deltaTime;
    mouseDelta *= 0.9f;

    if (pressing_up)
        rotationAngles.x -= rotationSpeed;
    if (pressing_down)
        rotationAngles.x += rotationSpeed;
    if (pressing_left)
        rotationAngles.y -= rotationSpeed;
    if (pressing_right)
        rotationAngles.y += rotationSpeed;

    rotationAngles.x = glm::clamp(rotationAngles.x, -89.0f, 89.0f);

    if (!cubePOVMode)
    {
        glm::vec2 mousePos = GetMouse::getMousePosition(window);
        bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);
        if (isDragging && isMouseOverCube)
        {
            glm::vec2 delta = mousePos - lastMousePos;
            SquarePos.x += delta.x * 0.002f * deltaTime * 60.0f;
            SquarePos.z -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f) * deltaTime * 60.0f;
        }
        lastMousePos = mousePos;
    }

    SimpleGravity(deltaTime, model, planes);

    model = glm::translate(glm::mat4(1.0f), SquarePos);
    model = glm::rotate(model, glm::radians(rotationAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));

    if (mode == debug || mode == release)
    {
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 view = glm::lookAt(cameraPos, cubePOVMode ? cameraPos + direction : SquarePos, up);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    mvp = projection * view * model;
}

// Update frame timing
void updateFrameTiming(double &previousTime, double &deltaTime, double &accumulator)
{
    double currentTime = glfwGetTime();
    deltaTime = currentTime - previousTime;
    previousTime = currentTime;
    deltaTime = std::min(deltaTime, 0.1);
    accumulator += deltaTime;

    frameCount++;
    if (currentTime - lastTime >= 1.0)
    {
        currentFPS = frameCount / (currentTime - lastTime);
        frameCount = 0;
        lastTime = currentTime;
    }
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
    for (const auto &plane : planes)
    {
        glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), plane.position);
        glm::mat4 planeMVP = projection * view * planeModel;
        glUniform1i(useTextureLocation, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, planeTexture);
        glUniform1i(textureLocation, 0);
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
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        glUniform1i(textureLocation, 0);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cubeMVP));
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    // Render text
    glDisable(GL_DEPTH_TEST);
    int collidingPlaneIndex;
    bool isColliding = isCubeCollidingWithPlane(model, planes, collidingPlaneIndex);
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    glm::mat4 cubeMVP = projection * view * model;
    bool isMouseOverCube = isPointInCube(mousePos, cubeMVP, width, height);

    if (renderDebugText)
    {
        char debugText[128];
        std::string retString = fmt::format("Cube position: ({:.2f}, {:.2f}, {:.2f}) [Rotation: ({:.1f}, {:.1f})]",
                                            SquarePos.x, SquarePos.y, SquarePos.z, rotationAngles.x, rotationAngles.y);
        snprintf(debugText, sizeof(debugText), "%s", retString.c_str());
        textRenderer->renderText(debugText, 10.0f, 10.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
        snprintf(debugText, sizeof(debugText), "OpenGL: %s", glGetString(GL_VERSION));
        textRenderer->renderText(debugText, width - 240.0f, height - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    if (isMouseOverCube && !cubePOVMode)
    {
        char cursorText[64];
        snprintf(cursorText, sizeof(cursorText), "Cursor position: (%.2f, %.2f)", mousePos.x, mousePos.y);
        textRenderer->renderText(cursorText, 10.0f, 50.0f, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (isColliding)
    {
        char collisionText[64];
        snprintf(collisionText, sizeof(collisionText), "Colliding with plane %d!", collidingPlaneIndex);
        textRenderer->renderText(collisionText, 10.0f, 70.0f, 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    char fpsText[16];
    snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", currentFPS);
    textRenderer->renderText(fpsText, 10.0f, height - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

    char hardwareText[128];
    snprintf(hardwareText, sizeof(hardwareText), "GPU: %s", glGetString(GL_RENDERER));
    textRenderer->renderText(hardwareText, 10.0f, height - 50.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

    glEnable(GL_DEPTH_TEST);
}

// Cleanup
void cleanup(GLuint program, GLuint cubeVAO, GLuint cubeVBO, GLuint cubeEBO, std::vector<Plane> &planes)
{
    glDeleteTextures(1, &planeTexture);
    glDeleteTextures(1, &cubeTexture);

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    for (auto &plane : planes)
    {
        glDeleteVertexArrays(1, &plane.vao);
        glDeleteBuffers(1, &plane.vbo);
        glDeleteBuffers(1, &plane.ebo);
    }

    glDeleteProgram(program);
    delete textRenderer;
    glfwTerminate();
}

// Main function
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        targetFPS = std::atoi(argv[1]);
        if (targetFPS <= 0)
        {
            spdlog::warn("Invalid FPS value provided. Using default: 144");
            targetFPS = 144;
        }
        spdlog::info("Target FPS: {}", targetFPS);
        targetFrameTime = 1.0 / targetFPS;
    }

    rotationAngles.x = 45.0f;
    rotationAngles.y = 0.0f;

    GLFWwindow *window = initializeWindow();

    // Setup shaders
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

    GLuint program = glCreateProgram();
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

    GLint mvp_location = glGetUniformLocation(program, "MVP");
    GLint vpos_location = glGetAttribLocation(program, "vPos");
    GLint vcol_location = glGetAttribLocation(program, "vCol");
    GLint vtex_location = glGetAttribLocation(program, "vTexCoord");
    textureLocation = glGetUniformLocation(program, "textureSampler");

    // Setup cube
    GLuint cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &cubeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    glEnableVertexAttribArray(vtex_location);
    glVertexAttribPointer(vtex_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));

    // Generate planes dynamically
    std::vector<Plane> planes;
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(0.0f, -1.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, -1.0f, 0.0f)});                       // Central
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(0.0f, 0.0f, 12.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)}); // North

    for (auto &plane : planes)
    {
        setupPlaneBuffers(plane, vpos_location, vcol_location, vtex_location);
    }

    // Load textures
    loadTexture("resources\\textures\\wood.jpg", planeTexture);
    loadTexture("resources\\textures\\concrete.jpg", cubeTexture);

    // Initialize text renderer
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
        cleanup(program, cubeVAO, cubeVBO, cubeEBO, planes);
        return -1;
    }

    double previousTime = glfwGetTime();
    glm::mat4 model;
    double accumulator = 0.0;
    const double fixedDeltaTime = 1.0 / 60.0;

    while (!glfwWindowShouldClose(window))
    {
        double deltaTime;
        updateFrameTiming(previousTime, deltaTime, accumulator);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;

        glm::mat4 mvp;
        while (accumulator >= fixedDeltaTime)
        {
            updateCube(window, model, mvp, width, height, ratio, (float)fixedDeltaTime, planes);
            accumulator -= fixedDeltaTime;
        }

        renderScene(window, program, mvp_location, cubeVAO, cubeEBO, planes, model, ratio);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup(program, cubeVAO, cubeVBO, cubeEBO, planes);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return 0;
}