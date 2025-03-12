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
#include <base64/base64.h> // You'll need a base64 decoding library (e.g., from https://github.com/ReneNyffenegger/cpp-base64)

#include "keyboard/keyboard.h"
#include "mouse/mouse_callback/mouse_callback.h"
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

// Global variables
static glm::vec3 SquarePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static glm::vec3 rotationAngles(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer = nullptr;
static GLuint planeTexture;
static GLint textureLocation;
static glm::vec3 cameraPos = glm::vec3(0.0f, -1.0f, 3.0f); // Start on the plane

// Cube data
static const Vertex vertices[8] = {
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}};

static const GLuint indices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
    5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
    3, 2, 6, 6, 7, 3, 0, 4, 5, 5, 1, 0};

// Plane data
static const Vertex planeVertices[4] = {
    {{-1.0f, 0.0f, -1.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
    {{1.0f, 0.0f, -1.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
    {{1.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
    {{-1.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}}};

static const GLuint planeIndices[6] = {0, 1, 2, 2, 3, 0};

// Shaders
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
    "        fragment = texture(textureSampler, texCoord);\n"
    "    } else {\n"
    "        fragment = vec4(color, 1.0);\n"
    "    }\n"
    "}\n";

// Error callback
static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// Mouse-cube intersection
bool isPointInCube(glm::vec2 mousePos, const glm::mat4 &mvp, int width, int height)
{
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

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize GLAD");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSwapInterval(0);
    glEnable(GL_DEPTH_TEST);
    return window;
}

// Setup OpenGL buffers and shaders
void setupRendering(GLuint &program, GLint &mvp_location, GLint &vpos_location, GLint &vcol_location,
                    GLuint &vertex_array, GLuint &vertex_buffer, GLuint &element_buffer,
                    GLuint &planeVertexArray, GLuint &planeVertexBuffer, GLuint &planeElementBuffer)
{

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

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    mvp_location = glGetUniformLocation(program, "MVP");
    vpos_location = glGetAttribLocation(program, "vPos");
    vcol_location = glGetAttribLocation(program, "vCol");
    GLint vtex_location = glGetAttribLocation(program, "vTexCoord");
    textureLocation = glGetUniformLocation(program, "textureSampler");
    GLint useTextureLocation = glGetUniformLocation(program, "useTexture");

    // Cube VAO
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    glDisableVertexAttribArray(vtex_location); // Cube doesn't use texture

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
    std::string texturePath = "resources/textures/wood.jpg";
    loadTexture(texturePath.c_str(), planeTexture);
}

// Initialize text renderer
void initializeTextRenderer()
{
    try
    {
        char bootDrive[MAX_PATH];
        GetWindowsDirectory(bootDrive, MAX_PATH);
        std::string bootDriveStr(bootDrive);
        std::string fontPath = bootDriveStr + "\\Fonts\\ARLRDBD.TTF";
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

// Handle input and update cube state
void updateCube(GLFWwindow *window, glm::mat4 &model, glm::mat4 &mvp, int width, int height, float ratio)
{
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);

    float rotationSpeed = 1.0f;
    if (KeyState::keyStates[GLFW_KEY_UP])
        rotationAngles.x += rotationSpeed;
    if (KeyState::keyStates[GLFW_KEY_DOWN])
        rotationAngles.x -= rotationSpeed;
    if (KeyState::keyStates[GLFW_KEY_LEFT])
        rotationAngles.y += rotationSpeed;
    if (KeyState::keyStates[GLFW_KEY_RIGHT])
        rotationAngles.y -= rotationSpeed;

    if (isDragging && isMouseOverCube)
    {
        glm::vec2 delta = mousePos - lastMousePos;
        SquarePos.x += delta.x * 0.002f;
        SquarePos.y -= delta.y * glm::clamp(0.002f * ratio, 0.002f, 0.01f);
        if (mode == debug)
            spdlog::info("Cube position: ({}, {})", SquarePos.x, SquarePos.y);
    }
    lastMousePos = mousePos;

    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x));
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);

    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, direction));

    float speed = 0.05f;
    if (pressing_w)
        SquarePos -= right * speed;
    if (pressing_s)
        SquarePos += right * speed;
    if (pressing_a)
        SquarePos -= direction * speed;
    if (pressing_d)
        SquarePos += direction * speed;

    model = glm::translate(glm::mat4(1.0f), SquarePos);
    model = glm::rotate(model, glm::radians(rotationAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
    // model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));

    if (mode == debug)
    {
        // rotate cube around its center
        model = glm::translate(model, glm::mat3(model) * glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + direction, up);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    mvp = projection * view * model;
}

// Calculate FPS and enforce frame rate
void updateFrameTiming(double currentTime, double &previousTime, double &deltaTime)
{
    currentTime = glfwGetTime();
    deltaTime = currentTime - previousTime;

    frameCount++;
    if (currentTime - lastTime >= 1.0)
    {
        currentFPS = frameCount / (currentTime - lastTime);
        frameCount = 0;
        lastTime = currentTime;
    }

    if (deltaTime < targetFrameTime)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((targetFrameTime - deltaTime) * 1000)));
        currentTime = glfwGetTime();
        deltaTime = currentTime - previousTime;

        frameCount++;
        if (currentTime - lastTime >= 1.0)
        {
            currentFPS = frameCount / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }

        if (currentFPS > targetFPS)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((1.0 / targetFPS - deltaTime) * 1000)));
            currentTime = glfwGetTime();
            deltaTime = currentTime - previousTime;
        }
    }

    previousTime = currentTime;
}

// Render scene
void renderScene(GLFWwindow *window, GLuint program, GLint mvp_location, GLuint vertex_array, GLuint element_buffer, GLuint planeVertexArray, GLuint planeElementBuffer, const glm::mat4 &model)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float ratio = width / (float)height;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    glm::mat4 mvp = projection * view * model;

    glUseProgram(program);

    // Render cube (use colors, no texture)
    glUniform1i(glGetUniformLocation(program, "useTexture"), 0);
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    // Render plane (with texture)
    glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    glm::mat4 planeMVP = projection * view * planeModel;
    glUniform1i(glGetUniformLocation(program, "useTexture"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planeTexture);
    glUniform1i(textureLocation, 0);
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(planeMVP));
    glBindVertexArray(planeVertexArray);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeElementBuffer);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Render text
    glDisable(GL_DEPTH_TEST);
    bool isColliding = isCubeCollidingWithPlane(model, vertices, planeVertices);
    glm::vec2 mousePos = GetMouse::getMousePosition(window);
    bool isMouseOverCube = isPointInCube(mousePos, mvp, width, height);

    if (renderDebugText)
    {
        char debugText[128];
        char versionText[64];
        std::string retString = fmt::format("Cube position: ({:.2f}, {:.2f}, {:.2f}) [Rotation: ({:.1f}, {:.1f})]",
                                            SquarePos.x, SquarePos.y, SquarePos.z, rotationAngles.x, rotationAngles.y);
        snprintf(debugText, sizeof(debugText), retString.c_str());
        textRenderer->renderText(debugText, 10.0f, 10.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
        snprintf(versionText, sizeof(versionText), "%s", glGetString(GL_VERSION));
        // display on the top right corner
        textRenderer->renderText(versionText, width - 170.0f, height - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    if (isMouseOverCube)
    {
        char cursorText[64];
        snprintf(cursorText, sizeof(cursorText), "Cursor position: (%.2f, %.2f)", mousePos.x, mousePos.y);
        textRenderer->renderText(cursorText, 10.0f, 50.0f, 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));

        // make the square go down when the mouse is over it
        if (!isDragging)
        {
            SquarePos.y -= 0.01f;
        }
    }

    if (isColliding)
    {
        char collisionText[64];
        snprintf(collisionText, sizeof(collisionText), "Cube collided with the plane!");
        textRenderer->renderText(collisionText, 10.0f, 70.0f, 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

        float lowestY = std::numeric_limits<float>::max();
        for (int i = 0; i < 8; i++)
        {
            glm::vec4 vertex(vertices[i].pos[0], vertices[i].pos[1], vertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = model * vertex;
            lowestY = std::min(lowestY, worldPos.y);
        }

        float planeY = -1.0f;
        if (lowestY < planeY)
        {
            float penetrationDepth = planeY - lowestY;
            SquarePos.y += penetrationDepth;
        }
    }

    char fpsText[16];
    snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", currentFPS);
    textRenderer->renderText(fpsText, 10.0f, height - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

    glEnable(GL_DEPTH_TEST);
}

// Cleanup resources
void cleanup(GLuint program, GLuint vertex_array, GLuint vertex_buffer, GLuint element_buffer,
             GLuint planeVertexArray, GLuint planeVertexBuffer, GLuint planeElementBuffer)
{
    glDeleteTextures(1, &planeTexture);
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
        if (std::string(argv[2]) == "debug")
        {
            mode = debug;
            renderDebugText = true;
            spdlog::info("Debug mode enabled");
        }
    }

    GLFWwindow *window = initializeWindow();

    GLuint program, vertex_array, vertex_buffer, element_buffer;
    GLuint planeVertexArray, planeVertexBuffer, planeElementBuffer;
    GLint mvp_location, vpos_location, vcol_location;
    setupRendering(program, mvp_location, vpos_location, vcol_location,
                   vertex_array, vertex_buffer, element_buffer,
                   planeVertexArray, planeVertexBuffer, planeElementBuffer);

    try
    {
        initializeTextRenderer();
    }
    catch (const std::exception &)
    {
        cleanup(program, vertex_array, vertex_buffer, element_buffer,
                planeVertexArray, planeVertexBuffer, planeElementBuffer);
        return -1;
    }

    double previousTime = glfwGetTime();
    glm::mat4 model;

    while (!glfwWindowShouldClose(window))
    {
        double currentTime = 0.0, deltaTime;
        updateFrameTiming(currentTime, previousTime, deltaTime);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;

        glm::mat4 mvp;
        updateCube(window, model, mvp, width, height, ratio);
        renderScene(window, program, mvp_location, vertex_array, element_buffer,
                    planeVertexArray, planeElementBuffer, model);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup(program, vertex_array, vertex_buffer, element_buffer,
            planeVertexArray, planeVertexBuffer, planeElementBuffer);
    return 0;
}