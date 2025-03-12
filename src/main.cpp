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

#include "keyboard/keyboard.h"
#include "mouse/mouse_callback/mouse_callback.h"
#include "mouse/mouse_position/get_mouse_position.h"

#include "render/vertex/vertex.h"
#include "render/text/text_renderer.h"
#include "config.h"
#include <filesystem>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Cube vertices
static const Vertex vertices[8] = {
    {{-0.5, -0.5, 0.5}, {1.0, 0.0, 0.0}},
    {{0.5, -0.5, 0.5}, {0.0, 1.0, 0.0}},
    {{0.5, 0.5, 0.5}, {0.0, 0.0, 1.0}},
    {{-0.5, 0.5, 0.5}, {1.0, 0.0, 1.0}},
    {{-0.5, -0.5, -0.5}, {1.0, 0.0, 0.0}},
    {{0.5, -0.5, -0.5}, {0.0, 1.0, 0.0}},
    {{0.5, 0.5, -0.5}, {0.0, 0.0, 1.0}},
    {{-0.5, 0.5, -0.5}, {1.0, 0.0, 1.0}},
};

// Cube indices
static const GLuint indices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
    5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
    3, 2, 6, 6, 7, 3, 0, 4, 5, 5, 1, 0};

// Cube shaders
static const char *vertex_shader_text =
    "#version 330\n"
    "uniform mat4 MVP;\n"
    "in vec3 vCol;\n"
    "in vec3 vPos;\n"
    "out vec3 color;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = MVP * vec4(vPos, 1.0);\n"
    "    color = vCol;\n"
    "}\n";

static const char *fragment_shader_text =
    "#version 330\n"
    "in vec3 color;\n"
    "out vec4 fragment;\n"
    "void main()\n"
    "{\n"
    "    fragment = vec4(color, 1.0);\n"
    "}\n";

static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

bool isPointInCube(glm::vec2 mousePos, const glm::mat4 &mvp, int width, int height)
{
    // Project cube vertices to screen space
    glm::vec4 screenVertices[8];
    for (int i = 0; i < 8; i++)
    {
        glm::vec4 vertex(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2], 1.0f);
        screenVertices[i] = mvp * vertex;

        // Convert from clip space to screen space
        screenVertices[i] /= screenVertices[i].w;
        screenVertices[i].x = (screenVertices[i].x + 1.0f) * width * 0.5f;
        screenVertices[i].y = (1.0f - screenVertices[i].y) * height * 0.5f;
    }

    // Check if mouse is within each triangle of the cube's faces
    auto pointInTriangle = [](glm::vec2 p, glm::vec2 v1, glm::vec2 v2, glm::vec2 v3)
    {
        float d1 = (p.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (p.y - v2.y);
        float d2 = (p.x - v3.x) * (v2.y - v3.y) - (v2.x - v3.x) * (p.y - v3.y);
        float d3 = (p.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (p.y - v1.y);
        bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(has_neg && has_pos);
    };

    // Define the cube's faces using the vertices
    glm::vec2 screenVertices2D[8];
    for (int i = 0; i < 8; i++)
    {
        screenVertices2D[i] = glm::vec2(screenVertices[i].x, screenVertices[i].y);
    }

    // Check each face of the cube
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

// Global variables
static glm::vec3 trianglePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static glm::vec3 rotationAngles(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer;

int main(int argc, char *argv[])
{
    // Override default FPS with command-line argument if provided
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
        return -1;
    }

    glfwSetWindowSizeLimits(window, 320, 240, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize GLAD");
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(0);
    glEnable(GL_DEPTH_TEST);

    // Setup cube
    GLuint vertex_buffer, element_buffer, vertex_array;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &element_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint mvp_location = glGetUniformLocation(program, "MVP");
    GLint vpos_location = glGetAttribLocation(program, "vPos");
    GLint vcol_location = glGetAttribLocation(program, "vCol");

    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));

    double previousTime = glfwGetTime();

    if (textRenderer == nullptr)
    {
        try
        {
            char bootDrive[MAX_PATH];
            GetWindowsDirectory(bootDrive, MAX_PATH);
            std::string bootDriveStr(bootDrive);
            std::string fontPath = bootDriveStr + "\\Fonts\\arial.ttf";
            spdlog::info("Font path: {}", fontPath);

            // Check if the file exists
            if (!std::filesystem::exists(fontPath))
            {
                spdlog::error("Font file not found: {}", fontPath);
                // Fallback to a different font or error handling (see below)
                fontPath = bootDriveStr + "\\Fonts\\times.ttf"; // Try Times New Roman as fallback
                if (!std::filesystem::exists(fontPath))
                {
                    spdlog::error("Fallback font not found either!");
                    glfwTerminate();
                    return -1;
                }
            }

            textRenderer = new TextRenderer(fontPath.c_str(), 32);
            keyboardTextRenderer = textRenderer; // Sync the pointers
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to initialize TextRenderer: {}", e.what());
            glfwTerminate();
            return -1;
        }
    }

    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - previousTime;

        // FPS calculation
        frameCount++;
        if (currentTime - lastTime >= 1.0)
        {
            currentFPS = frameCount / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }

        // Frame limiting to target FPS
        if (deltaTime < targetFrameTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((targetFrameTime - deltaTime) * 1000)));
            currentTime = glfwGetTime();
            deltaTime = currentTime - previousTime;

            // Recalculate FPS
            frameCount++;
            if (currentTime - lastTime >= 1.0)
            {
                currentFPS = frameCount / (currentTime - lastTime);
                frameCount = 0;
                lastTime = currentTime;
            }

            // if over 144 FPS then ensure it stays at 144
            if (currentFPS > targetFPS)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((1.0 / targetFPS - deltaTime) * 1000)));
                currentTime = glfwGetTime();
                deltaTime = currentTime - previousTime;
            }
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Handle mouse dragging
        glm::vec2 mousePos = GetMouse::getMousePosition(window);

        float rotationSpeed = 1.0f;
        if (KeyState::keyStates[GLFW_KEY_UP])
            rotationAngles.x += rotationSpeed;
        if (KeyState::keyStates[GLFW_KEY_DOWN])
            rotationAngles.x -= rotationSpeed;
        if (KeyState::keyStates[GLFW_KEY_LEFT])
            rotationAngles.y += rotationSpeed;
        if (KeyState::keyStates[GLFW_KEY_RIGHT])
            rotationAngles.y -= rotationSpeed;

        // Render cube
        glm::mat4 model = glm::translate(glm::mat4(1.0f), trianglePos);
        model = glm::rotate(model, glm::radians(rotationAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotationAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
        glm::mat4 mvp = projection * view * model;

        bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);

        // Handle mouse dragging
        if (isDragging && isMouseOverCube)
        {
            glm::vec2 delta = mousePos - lastMousePos;
            trianglePos.x += delta.x * 0.002f;
            trianglePos.y -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f);
            if (mode == debug)
                spdlog::info("Cube position: ({}, {})", trianglePos.x, trianglePos.y);
        }
        lastMousePos = mousePos;

        if (pressing_w)
            trianglePos.z += 0.01f;
        if (pressing_s)
            trianglePos.z -= 0.01f;
        if (pressing_a)
            trianglePos.x -= 0.01f;
        if (pressing_d)
            trianglePos.x += 0.01f;

        // Render the cube first with depth testing enabled
        glUseProgram(program);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Disable depth testing for text rendering to ensure it appears on top
        glDisable(GL_DEPTH_TEST);

        // Render debug text when flag is set
        if (renderDebugText)
        {
            char debugText[128];
            std::string retString = fmt::format("Cube position: ({}, {}) ", trianglePos.x, trianglePos.y);
            std::string retString2 = fmt::format("Cube rotation: ({}, {}) ", rotationAngles.x, rotationAngles.y);
            retString += retString2;
            snprintf(debugText, sizeof(debugText), retString.c_str());
            // White
            textRenderer->renderText(debugText, 10.0f, 10.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
        }

        // Render cursor position only when inside cube
        if (isMouseOverCube)
        {
            char cursorText[64];
            snprintf(cursorText, sizeof(cursorText), "Cursor position: (%.2f, %.2f)", mousePos.x, mousePos.y);
            textRenderer->renderText(cursorText, 10.0f, 50.0f, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        // Render FPS text
        char fpsText[16];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", currentFPS);
        textRenderer->renderText(fpsText, 10.0f, 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

        // Re-enable depth testing for the next frame
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
        previousTime = currentTime;
    }

    glDeleteVertexArrays(1, &vertex_array);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &element_buffer);
    glDeleteProgram(program);
    delete textRenderer;
    glfwTerminate();
    return 0;
}