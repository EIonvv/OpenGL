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

#include "keyboard/keyboard.h"
#include "mouse/mouse_callback/mouse_callback.h"
#include "mouse/mouse_position/get_mouse_position.h"

#include "render/vertex/vertex.h"
#include "render/text/text_renderer.h"

#include "config.h"

// Initial target FPS (can be overridden by command-line argument)
static int targetFPS = 144;
static double targetFrameTime = 1.0 / targetFPS;

// Cube vertices
static const Vertex vertices[8] = {
    {{-0.5, -0.5, 0.5}, {1.0, 0.0, 0.0}}, {{0.5, -0.5, 0.5}, {0.0, 1.0, 0.0}}, {{0.5, 0.5, 0.5}, {0.0, 0.0, 1.0}}, {{-0.5, 0.5, 0.5}, {1.0, 0.0, 1.0}}, {{-0.5, -0.5, -0.5}, {1.0, 0.0, 0.0}}, {{0.5, -0.5, -0.5}, {0.0, 1.0, 0.0}}, {{0.5, 0.5, -0.5}, {0.0, 0.0, 1.0}}, {{-0.5, 0.5, -0.5}, {1.0, 0.0, 1.0}}};

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

// Global variables
static glm::vec3 trianglePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static glm::vec3 rotationAngles(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer;

void displayFPS(GLFWwindow *window)
{
    char title[64];
    snprintf(title, sizeof(title), "3D Draggable Cube - Target: %d, FPS: %.1f", targetFPS, currentFPS);
    glfwSetWindowTitle(window, title);
}

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
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Handle mouse dragging
        glm::vec2 mousePos = GetMouse::getMousePosition(window);
        if (isDragging)
        {
            glm::vec2 delta = mousePos - lastMousePos;
            trianglePos.x += delta.x * 0.002f;
            trianglePos.y -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f);
            if (mode == debug)
                spdlog::info("Cube position: ({}, {})", trianglePos.x, trianglePos.y);
        }
        lastMousePos = mousePos;

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
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
        glm::mat4 mvp = projection * view * model;

        if (textRenderer == nullptr)
        {
            try
            {
                textRenderer = new TextRenderer("C:/Windows/Fonts/ARLRDBD.ttf", 48);
                spdlog::info("TextRenderer initialized successfully");
            }
            catch (const std::exception &e)
            {
                spdlog::error("Failed to initialize TextRenderer: {}", e.what());
                glfwTerminate();
                return -1;
            }
        }

        // Render FPS text
        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", currentFPS);
        textRenderer->renderText(fpsText, 10.0f, 30.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));

        glUseProgram(program);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
        previousTime = currentTime;
    }

    glDeleteVertexArrays(1, &vertex_array);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &element_buffer);
    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}