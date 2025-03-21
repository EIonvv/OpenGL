#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <chrono>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/msvc_sink.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <base64/base64.h>

// Include Tracy header
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
#include "render/setup/setupRenderer.h"

// Global variables
static glm::vec3 SquarePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer = nullptr;

// Start at the plane just in front of the cube
static glm::vec3 cameraPos = glm::vec3(5.0f, 0.0f, 5.0f);
static bool cubePOVMode = false; // New flag for cube POV mode

// Error callback
static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// Mouse-cube intersection
bool isPointInCube(glm::vec2 mousePos, const glm::mat4 &mvp, int width, int height)
{
    ZoneScoped; // Tracy: Profile this function
    glm::vec4 screenVertices[8];
    for (int i = 0; i < 8; i++)
    {
        glm::vec4 vertex(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2], 1.0f);
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
bool isCubeCollidingWithPlane(const glm::mat4 &model, const Vertex *cubeVertices, const Vertex *planeVertices)
{
    ZoneScoped; // Tracy: Profile this function
    std::array<glm::vec3, 8> cubeWorldVertices;
    for (int i = 0; i < 8; i++)
    {
        glm::vec4 vertex(cubeVertices[i].pos[0], cubeVertices[i].pos[1], cubeVertices[i].pos[2], 1.0f);
        glm::vec4 worldPos = model * vertex;
        cubeWorldVertices[i] = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
    }

    std::array<glm::vec3, 4> planeWorldVertices;
    glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    for (int i = 0; i < 4; i++)
    {
        glm::vec4 vertex(planeVertices[i].pos[0], planeVertices[i].pos[1], planeVertices[i].pos[2], 1.0f);
        glm::vec4 worldPos = planeModel * vertex;
        planeWorldVertices[i] = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
    }

    if (mode == debug)
    {
        // spdlog::info("Plane vertices (world space):");
        // for (int i = 0; i < 4; i++) {
        //     spdlog::info("Vertex {}: ({:.2f}, {:.2f}, {:.2f})", i, planeWorldVertices[i].x, planeWorldVertices[i].y, planeWorldVertices[i].z);
        // }
    }

    glm::vec3 edge1 = planeWorldVertices[1] - planeWorldVertices[0];
    glm::vec3 edge2 = planeWorldVertices[2] - planeWorldVertices[0];
    glm::vec3 planeNormal = glm::normalize(glm::cross(edge1, edge2));

    std::array<glm::vec3, 3> cubeNormals = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)};

    glm::mat3 normalMatrix = glm::mat3(model);
    for (auto &normal : cubeNormals)
    {
        normal = glm::normalize(normalMatrix * normal);
    }

    std::array<glm::vec3, 4> testAxes = {
        planeNormal,
        cubeNormals[0],
        cubeNormals[1],
        cubeNormals[2]};

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
}

// Initialize GLFW and OpenGL
GLFWwindow *initializeWindow()
{
    ZoneScoped; // Tracy: Profile this function
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
    glfwSetCursorPosCallback(window, mouse_callback); // Add mouse movement callback

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

void SimpleGravity(float deltaTime, glm::mat4 &model, const Vertex *vertices, const Vertex *planeVertices)
{
    // Gravity and collision
    SquarePos.y -= 0.1f * static_cast<float>(deltaTime); // Simple gravity
    if (isCubeCollidingWithPlane(model, vertices, planeVertices))
    {
        float lowestY = std::numeric_limits<float>::max();
        for (int i = 0; i < 8; i++)
        {
            glm::vec4 vertex(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = model * vertex;
            lowestY = std::min(lowestY, worldPos.y);
        }
        float planeY = -1.0f;
        if (lowestY < planeY)
            SquarePos.y += (planeY - lowestY);
    }
}

// Handle input and update cube state
void updateCube(GLFWwindow *window, glm::mat4 &model, glm::mat4 &mvp, int width, int height, float ratio, float deltaTime)
{
    ZoneScoped; // Tracy: Profile this function
    // Camera and cube movement
    float baseSpeed = static_cast<float>(deltaTime) * 12.0f; // Base speed of 12 units per second
    float speed = baseSpeed;                                 // Default speed, adjusted for POV mode if needed

    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x)); // Pitch affects vertical look
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);

    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // Ensure up vector is consistent

    if (cubePOVMode)
    {
        speed = static_cast<float>(deltaTime) * 3.0f;        // Slower speed in POV mode for precision
        SquarePos.y = -0.5f;                                 // Keep cube on the plane (plane at y = -1, cube height = 1)
        cameraPos = SquarePos + glm::vec3(0.0f, 0.5f, 0.0f); // Camera at cube center
    }

    // Use WASD for movement in both modes
    if (pressing_w)
    {
        if (cubePOVMode)
            SquarePos += direction * speed * (float)deltaTime * 60.0f; // Speed up in POV mode
        else
            cameraPos += direction * speed;
    }
    if (pressing_s)
    {
        if (cubePOVMode)
            SquarePos -= direction * speed * (float)deltaTime * 60.0f; // Speed up in POV mode
        else
            cameraPos -= direction * speed;
    }
    if (pressing_a)
    {
        if (cubePOVMode)
            SquarePos -= right * speed * (float)deltaTime * 60.0f; // Speed up in POV mode
        else
            cameraPos -= right * speed;
    }
    if (pressing_d)
    {
        if (cubePOVMode)
            SquarePos += right * speed * (float)deltaTime * 60.0f; // Speed up in POV mode
        else
            cameraPos += right * speed;
    }

    // Smooth rotation with mouse
    float rotationSpeed = 1.0f;                                                       // Degrees per second per pixel
    rotationAngles.y += static_cast<float>(mouseDelta.x) * rotationSpeed * deltaTime; // Yaw
    rotationAngles.x -= static_cast<float>(mouseDelta.y) * rotationSpeed * deltaTime; // Pitch (inverted)
    mouseDelta *= 0.9f;

    // Dampen delta over time (adjustable)
    if (pressing_up)
        rotationAngles.x -= rotationSpeed; // Pitch up
    if (pressing_down)
        rotationAngles.x += rotationSpeed; // Pitch down
    if (pressing_left)
        rotationAngles.y -= rotationSpeed; // Yaw left
    if (pressing_right)
        rotationAngles.y += rotationSpeed; // Yaw right

    // Clamp pitch
    rotationAngles.x = glm::clamp(rotationAngles.x, -89.0f, 89.0f);

    if (!cubePOVMode)
    {
        // Mouse dragging in free mode
        glm::vec2 mousePos = GetMouse::getMousePosition(window);
        bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);
        if (isDragging && isMouseOverCube)
        {
            glm::vec2 delta = mousePos - lastMousePos;
            SquarePos.x += delta.x * 0.002f * static_cast<float>(deltaTime) * 60.0f;
            SquarePos.z -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f) * static_cast<float>(deltaTime) * 60.0f;
        }
        lastMousePos = mousePos;
    }

    // Mouse dragging in POV mode (optional, can be adjusted or removed)
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);
    if (isDragging && isMouseOverCube)
    {
        glm::vec2 delta = mousePos - lastMousePos;
        SquarePos.x += delta.x * 0.002f;
        SquarePos.z -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f);
        if (mode == debug)
            spdlog::info("Cube position: ({}, {}, {})", SquarePos.x, SquarePos.y, SquarePos.z);
    }
    lastMousePos = mousePos;

    // Simple gravity and collision
    SimpleGravity(deltaTime, model, vertices, planeVertices);

    model = glm::translate(glm::mat4(1.0f), SquarePos);
    model = glm::rotate(model, glm::radians(rotationAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
    if (mode == debug)
    {
        model = glm::translate(model, glm::mat3(model) * glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (mode == release)
    {
        model = glm::translate(model, glm::mat3(model) * glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // invert the camera position for the cube POV mode
    glm::mat4 view = glm::lookAt(cameraPos, SquarePos, up / 2.0f);

    // look in the direction of the cube
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    mvp = projection * view * model;
}

// Calculate FPS and enforce frame rate
void updateFrameTiming(double &previousTime, double &deltaTime, double &accumulator)
{
    ZoneScoped; // Tracy: Profile this function
    double currentTime = glfwGetTime();
    deltaTime = currentTime - previousTime;
    previousTime = currentTime;

    // Cap deltaTime to avoid large jumps (e.g., when window is moved)
    deltaTime = std::min(deltaTime, 0.1); // Max 100ms per frame

    accumulator += deltaTime;

    // FPS calculation
    frameCount++;
    if (currentTime - lastTime >= 1.0)
    {
        currentFPS = frameCount / (currentTime - lastTime);
        frameCount = 0;
        lastTime = currentTime;
    }
}

// Render the scene
void renderScene(GLFWwindow *window, GLuint program, GLint mvp_location, GLuint vertex_array, GLuint element_buffer, GLuint planeVertexArray, GLuint planeElementBuffer, const glm::mat4 &model, float ratio)
{
    ZoneScoped; // Tracy: Profile this function
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use camera-based view
    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x));
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);                         // Up vector (assuming plane is horizontal)
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + direction, up); // Adjusted to look in direction
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);

    glUseProgram(program);
    GLint useTextureLocation = glGetUniformLocation(program, "useTexture");

    // Render plane (with texture)
    glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    glm::mat4 planeMVP = projection * view * planeModel;
    glUniform1i(useTextureLocation, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planeTexture);
    glUniform1i(textureLocation, 0);
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(planeMVP));
    glBindVertexArray(planeVertexArray);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeElementBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Render cube only if not in cubePOVMode
    if (!cubePOVMode)
    {
        // Render cube (with texture)
        glm::mat4 cubeMVP = projection * view * model;
        glUniform1i(useTextureLocation, 1); // Important to enable texture or else it will render as gray or black
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        glUniform1i(textureLocation, 0);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cubeMVP));
        glBindVertexArray(vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    // Render text
    glDisable(GL_DEPTH_TEST);
    bool isColliding = isCubeCollidingWithPlane(model, vertices, planeVertices);
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    glm::mat4 cubeMVP = projection * view * model; // For mouse intersection check
    bool isMouseOverCube = isPointInCube(mousePos, cubeMVP, width, height);

    if (renderDebugText)
    {
        char debugText[128];
        char versionText[64];
        std::string retString = fmt::format("Cube position: ({:.2f}, {:.2f}, {:.2f}) [Rotation: ({:.1f}, {:.1f})]",
                                            SquarePos.x, SquarePos.y, SquarePos.z, rotationAngles.x, rotationAngles.y);
        snprintf(debugText, sizeof(debugText), "%s", retString.c_str());
        textRenderer->renderText(debugText, 10.0f, 10.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
        snprintf(versionText, sizeof(versionText), "%s", glGetString(GL_VERSION));
        textRenderer->renderText(versionText, width - 170.0f, height - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    if (isMouseOverCube && !cubePOVMode) // Only show if not in POV mode
    {
        char cursorText[64];
        snprintf(cursorText, sizeof(cursorText), "Cursor position: (%.2f, %.2f)", mousePos.x, mousePos.y);
        textRenderer->renderText(cursorText, 10.0f, 50.0f, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (isColliding)
    {
        char collisionText[64];
        snprintf(collisionText, sizeof(collisionText), "Cube collided with the plane!");
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

// Cleanup resources
void cleanup(GLuint program, GLuint vertex_array, GLuint vertex_buffer, GLuint element_buffer,
             GLuint planeVertexArray, GLuint planeVertexBuffer, GLuint planeElementBuffer)
{
    ZoneScoped; // Tracy: Profile this function
    glDeleteTextures(1, &planeTexture);
    glDeleteTextures(1, &cubeTexture); // Delete cube texture
    glDeleteVertexArrays(1, &vertex_array);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &element_buffer);
    glDeleteVertexArrays(1, &planeVertexArray);
    glDeleteBuffers(1, &planeVertexBuffer);
    glDeleteBuffers(1, &planeElementBuffer);
    glDeleteProgram(program);
    delete textRenderer;
    glfwTerminate();
}

// Main function
int main(int argc, char *argv[])
{
    ZoneScoped; // Tracy: Profile the main function
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

    if (argc > 2)
    {
        if (std::string(argv[2]) == "debug" ? mode = debug : mode = release)
            spdlog::info("Mode: {}", mode);
        if (mode == release)
        {
            spdlog::set_level(spdlog::level::info);
        }
    }

    // Set initial rotation to look downward at the plane
    rotationAngles.x = 45.0f; // Pitch down 45 degrees to see the plane
    rotationAngles.y = 0.0f;  // Yaw straight ahead

    GLFWwindow *window = initializeWindow();

    GLuint program, vertex_array, vertex_buffer, element_buffer;
    GLuint planeVertexArray, planeVertexBuffer, planeElementBuffer;
    GLint mvp_location, vpos_location, vcol_location;
    setupRendering(program, mvp_location, vpos_location, vcol_location,
                   vertex_array, vertex_buffer, element_buffer,
                   planeVertexArray, planeVertexBuffer, planeElementBuffer);

    try
    {
        initializeTextRenderer((TextRenderer *&)textRenderer);
    }
    catch (const std::exception &)
    {
        cleanup(program, vertex_array, vertex_buffer, element_buffer,
                planeVertexArray, planeVertexBuffer, planeElementBuffer);
        return -1;
    }

    double previousTime = glfwGetTime();
    glm::mat4 model;
    double accumulator = 0.0;
    const double fixedDeltaTime = 1.0 / 60.0; // Fixed 60 Hz update rate for physics/movement

    while (!glfwWindowShouldClose(window))
    {
        ZoneScoped; // Tracy: Profile the main loop iteration
        double deltaTime;

        updateFrameTiming(previousTime, deltaTime, accumulator);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;

        glm::mat4 mvp;

        // Process updates in fixed steps
        while (accumulator >= fixedDeltaTime)
        {
            updateCube(window, model, mvp, width, height, ratio, (float)fixedDeltaTime);
            accumulator -= fixedDeltaTime;
        }

        renderScene(window, program, mvp_location, vertex_array, element_buffer,
                    planeVertexArray, planeElementBuffer, model, ratio);

        glfwSwapBuffers(window);
        glfwPollEvents();

        FrameMark; // Tracy: Mark the end of a frame
    }

    cleanup(program, vertex_array, vertex_buffer, element_buffer,
            planeVertexArray, planeVertexBuffer, planeElementBuffer);
    return 0;
}