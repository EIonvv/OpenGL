#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
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
#include "render/scene/structures/vertex/vertex.h"
#include "render/text/text_renderer.h"
#include "render/texture/texture.h"
#include "globals.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#include "render/setupRenderer.h"
#include <imgui_impl_opengl3.h>
#include "render/imGui/debug_imgui.h"
#include "render/scene/render_scene.h"
#include "render/shader/shader.h"
#include "render/scene/structures/cube/cube_struct.h"

#include <unordered_map>

// Define a simple 2D grid position (x, z) for tracking
struct GridPos
{
    int x, z;
    bool operator==(const GridPos &other) const
    {
        return x == other.x && z == other.z;
    }
};

// Hash function for GridPos to use in unordered_map
struct GridPosHash
{
    std::size_t operator()(const GridPos &pos) const
    {
        return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.z) << 1);
    }
};

// Bounding Box structure
struct BoundingBox {
    glm::vec3 min, max;
};

// Global world bounds
BoundingBox worldBounds;

// Map to store visit counts for each grid cell
std::unordered_map<GridPos, int, GridPosHash> visitedAreas;

// Grid cell size (adjust based on your world scale)
const float GRID_CELL_SIZE = 1.0f;

// Error callback
static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// Generate plane vertices with rotation
std::vector<Vertex> generatePlaneVertices(float width, float depth, const glm::vec3 &position, const glm::vec3 &rotation)
{
    std::vector<Vertex> vertices(4);
    float halfWidth = width / 2.0f;
    float halfDepth = depth / 2.0f;

    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec4 bottomLeft = rotationMatrix * glm::vec4(-halfWidth, 0.0f, -halfDepth, 1.0f);
    glm::vec4 bottomRight = rotationMatrix * glm::vec4(halfWidth, 0.0f, -halfDepth, 1.0f);
    glm::vec4 topRight = rotationMatrix * glm::vec4(halfWidth, 0.0f, halfDepth, 1.0f);
    glm::vec4 topLeft = rotationMatrix * glm::vec4(-halfWidth, 0.0f, halfDepth, 1.0f);

    vertices[0] = {{position.x + bottomLeft.x, position.y + bottomLeft.y, position.z + bottomLeft.z}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}};
    vertices[1] = {{position.x + bottomRight.x, position.y + bottomRight.y, position.z + bottomRight.z}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}};
    vertices[2] = {{position.x + topRight.x, position.y + topRight.y, position.z + topRight.z}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}};
    vertices[3] = {{position.x + topLeft.x, position.y + topLeft.y, position.z + topLeft.z}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}};

    spdlog::info("Bottom-left: ({}, {}, {})", position.x + bottomLeft.x, position.y + bottomLeft.y, position.z + bottomLeft.z);
    spdlog::info("Bottom-right: ({}, {}, {})", position.x + bottomRight.x, position.y + bottomRight.y, position.z + bottomRight.z);
    spdlog::info("Top-right: ({}, {}, {})", position.x + topRight.x, position.y + topRight.y, position.z + topRight.z);
    spdlog::info("Top-left: ({}, {}, {})", position.x + topLeft.x, position.y + topLeft.y, position.z + topLeft.z);

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

// Simple gravity function
void SimpleGravity(float deltaTime, glm::mat4 &model, const std::vector<Plane> &planes)
{
    glm::vec3 gravityDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    float gravityStrength = 9.8f;

    if (isCubeCollidingWithPlane(model, planes, collidingPlaneIndex))
    {
        const Plane &collidingPlane = planes[collidingPlaneIndex];
        glm::vec3 edge1 = glm::make_vec3(collidingPlane.vertices[1].pos) - glm::make_vec3(collidingPlane.vertices[0].pos);
        glm::vec3 edge2 = glm::make_vec3(collidingPlane.vertices[2].pos) - glm::make_vec3(collidingPlane.vertices[0].pos);
        glm::vec3 planeNormal = glm::normalize(glm::cross(edge1, edge2));
        gravityDirection = -planeNormal;

        std::array<glm::vec3, 8> cubeWorldVertices;
        for (int i = 0; i < 8; i++)
        {
            glm::vec4 vertex(cubeVertices[i].pos[0], cubeVertices[i].pos[1], cubeVertices[i].pos[2], 1.0f);
            glm::vec4 worldPos = model * vertex;
            cubeWorldVertices[i] = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
        }

        float minPenetration = std::numeric_limits<float>::max();
        for (const auto &vertex : cubeWorldVertices)
        {
            glm::vec3 toVertex = vertex - glm::make_vec3(collidingPlane.vertices[0].pos);
            float distance = glm::dot(toVertex, planeNormal);
            if (distance < 0.0f)
            {
                minPenetration = std::min(minPenetration, std::abs(distance));
            }
        }

        if (minPenetration != std::numeric_limits<float>::max())
        {
            SquarePos += planeNormal * minPenetration * (float)deltaTime * 0.01f;
            model = glm::translate(glm::mat4(1.0f), SquarePos);
        }
    }

    SquarePos += gravityDirection * gravityStrength * deltaTime * 0.01f;
    model = glm::translate(glm::mat4(1.0f), SquarePos);
}

// Update cube function with bounding box optimization
void updateCube(GLFWwindow *window, glm::mat4 &model, glm::mat4 &mvp, int width, int height, float ratio, float deltaTime, const std::vector<Plane> &planes)
{
    float baseSpeed = deltaTime * 12.0f;
    float speed = baseSpeed;

    if (isBoostActive)
    {
        speed *= BOOST_MULTIPLIER;
    }

    glm::vec3 direction;
    direction.x = cos(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction.y = sin(glm::radians(rotationAngles.x));
    direction.z = sin(glm::radians(rotationAngles.y)) * cos(glm::radians(rotationAngles.x));
    direction = glm::normalize(direction);

    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 previousPos = SquarePos;

    if (cubePOVMode)
    {
        speed = deltaTime * 3.0f;
        cameraPos = SquarePos + glm::vec3(0.0f, 0.5f, 0.0f);

        if (isCubeCollidingWithPlane(model, planes, collidingPlaneIndex))
        {
            SquarePos.y = -0.5f;
        }

        lastSquarePos = SquarePos;
    }

    if (deltaTime > 0.1f)
    {
        speed = baseSpeed * 0.1f;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !isBoostActive)
    {
        isBoostActive = true;
        boostDistanceTraveled = 0.0f;
        spdlog::info("Boost activated!");
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

    float distanceThisFrame = glm::length(SquarePos - previousPos);

    if (isBoostActive)
    {
        boostDistanceTraveled += distanceThisFrame;
        if (boostDistanceTraveled >= BOOST_DURATION_DISTANCE)
        {
            isBoostActive = false;
            spdlog::info("Boost deactivated after traveling {} units", boostDistanceTraveled);
        }
    }

    float rotationSpeed = 1.0f;

    if (mouseInputEnabled)
    {
        rotationAngles.y += static_cast<float>(mouseDelta.x) * rotationSpeed * deltaTime;
        rotationAngles.x -= static_cast<float>(mouseDelta.y) * rotationSpeed * deltaTime;
        mouseDelta *= 0.9f;
    }

    if (pressing_up)
        rotationAngles.x -= rotationSpeed;
    if (pressing_down)
        rotationAngles.x += rotationSpeed;
    if (pressing_left)
        rotationAngles.y -= rotationSpeed;
    if (pressing_right)
        rotationAngles.y += rotationSpeed;

    rotationAngles.x = glm::clamp(rotationAngles.x, -89.0f, 89.0f);

    SimpleGravity(deltaTime, model, planes);

    if (!cubePOVMode)
    {
        wanderTimer += deltaTime;

        static glm::vec3 currentDirection = wanderDirection;
        static glm::vec3 targetDirection = wanderDirection;
        const float turnSpeed = 2.0f;
        float wanderSpeed = 5.0f;

        if (isBoostActive)
        {
            wanderSpeed *= BOOST_MULTIPLIER;
        }

        GridPos currentGridPos = {
            static_cast<int>(std::floor(SquarePos.x / GRID_CELL_SIZE)),
            static_cast<int>(std::floor(SquarePos.z / GRID_CELL_SIZE))};

        visitedAreas[currentGridPos]++;

        int visitCount = visitedAreas[currentGridPos];
        float adjustedWanderSpeed = glm::clamp(2.0f / (1.0f + visitCount * 0.5f), 1.0f, 5.0f);
        if (isBoostActive)
        {
            adjustedWanderSpeed *= BOOST_MULTIPLIER;
        }
        spdlog::info("Visit count at ({}, {}): {}, Speed: {}", currentGridPos.x, currentGridPos.z, visitCount, adjustedWanderSpeed);

        if (wanderTimer >= wanderChangeInterval)
        {
            float randomAngle = static_cast<float>(rand()) / RAND_MAX * 360.0f;
            glm::vec3 potentialTargetDirection = glm::normalize(glm::vec3(cos(glm::radians(randomAngle)), 0.0f, sin(glm::radians(randomAngle))));

            glm::vec3 potentialNewPos = SquarePos + potentialTargetDirection * adjustedWanderSpeed * deltaTime;

            // Check if potentialNewPos is within world bounds
            if (potentialNewPos.x >= worldBounds.min.x && potentialNewPos.x <= worldBounds.max.x &&
                potentialNewPos.z >= worldBounds.min.z && potentialNewPos.z <= worldBounds.max.z)
            {
                targetDirection = potentialTargetDirection;
            }
            else
            {
                targetDirection = -currentDirection;
            }

            wanderTimer = 0.0f;
        }

        currentDirection = glm::normalize(glm::mix(currentDirection, targetDirection, turnSpeed * deltaTime));
        glm::vec3 newPos = SquarePos + currentDirection * adjustedWanderSpeed * deltaTime;

        // Check if newPos is within world bounds
        if (newPos.x >= worldBounds.min.x && newPos.x <= worldBounds.max.x &&
            newPos.z >= worldBounds.min.z && newPos.z <= worldBounds.max.z)
        {
            SquarePos = newPos;
        }
        else
        {
            // If out of bounds, clamp to the nearest valid position and reflect direction
            SquarePos.x = glm::clamp(newPos.x, worldBounds.min.x + 0.1f, worldBounds.max.x - 0.1f);
            SquarePos.z = glm::clamp(newPos.z, worldBounds.min.z + 0.1f, worldBounds.max.z - 0.1f);
            SquarePos.y = newPos.y;

            glm::vec3 normal(0.0f, 0.0f, 0.0f);
            if (newPos.x <= worldBounds.min.x) normal.x = 1.0f;
            else if (newPos.x >= worldBounds.max.x) normal.x = -1.0f;
            if (newPos.z <= worldBounds.min.z) normal.z = 1.0f;
            else if (newPos.z >= worldBounds.max.z) normal.z = -1.0f;

            currentDirection = glm::reflect(currentDirection, glm::normalize(normal));
            targetDirection = currentDirection;
        }
    }

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

// Initialize GLFW and OpenGL
GLFWwindow *initializeWindow()
{
    if (mode == release)
    {
        FreeConsole();
    }

    glfwSetErrorCallback([](int error, const char *description)
                         { spdlog::error("GLFW Error {}: {}", error, description); });

    if (!glfwInit())
    {
        spdlog::critical("Failed to initialize GLFW. Terminating program.");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow *window = glfwCreateWindow(800, 600, "3D Draggable Cube", NULL, NULL);
    if (!window)
    {
        spdlog::error("Failed to create GLFW window. Possible reasons include lack of OpenGL context or incompatible hardware.");
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
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // glEnable(GL_BLEND);
    // glEnable(GL_DEPTH_TEST); // Enable depth testing once here
    glfwSwapInterval(1);

    spdlog::info("GLFW and OpenGL initialized successfully.");
    return window;
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

    planes.reserve(13); // Reserve space for all 13 planes
    cubes.reserve(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    if (io.Fonts->AddFontFromFileTTF("resources\\fonts\\arlrbd.ttf", 16.0f) == NULL)
    {
        spdlog::error("Failed to load font file");
        return -1;
    }
    else
    {
        spdlog::info("Font loaded successfully for ImGui");
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    GLuint program = createShaderProgram(vertex_shader_text, fragment_shader_text);
    GLint mvp_location = glGetUniformLocation(program, "MVP");
    GLint vpos_location = glGetAttribLocation(program, "vPos");
    GLint vcol_location = glGetAttribLocation(program, "vCol");
    GLint vtex_location = glGetAttribLocation(program, "vTexCoord");
    textureLocation = glGetUniformLocation(program, "textureSampler");

    Cube cube;
    cubes.push_back(cube);
    spdlog::info("Cube: {}", cubes.size());

    glGenVertexArrays(1, &cube.VAO);
    glBindVertexArray(cube.VAO);
    glGenBuffers(1, &cube.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, cube.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glGenBuffers(1, &cube.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, col));
    glEnableVertexAttribArray(vtex_location);
    glVertexAttribPointer(vtex_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));

    // Define planes
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, 0.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {0.0f, 0.0f, 12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {12.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {0.0f, 0.0f, -12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {-12.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {-12.0f, 0.0f, -12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {12.0f, 0.0f, 12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {12.0f, 0.0f, -12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {-12.0f, 0.0f, 12.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {0.0f, 0.0f, 24.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {0.0f, 0.0f, -24.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {24.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});
    planes.emplace_back(Plane{generatePlaneVertices(12.0f, 12.0f, {-24.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), 0, 0, 0, {0.0f, planes[0].position.y - 1.0f, 0.0f}});

    // Compute world bounds after defining planes
    worldBounds.min = glm::vec3(std::numeric_limits<float>::max());
    worldBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto &plane : planes)
    {
        for (const auto &vertex : plane.vertices)
        {
            worldBounds.min = glm::min(worldBounds.min, glm::vec3(vertex.pos[0], vertex.pos[1], vertex.pos[2]));
            worldBounds.max = glm::max(worldBounds.max, glm::vec3(vertex.pos[0], vertex.pos[1], vertex.pos[2]));
        }
    }
    spdlog::info("World bounds: min ({}, {}, {}), max ({}, {}, {})",
                 worldBounds.min.x, worldBounds.min.y, worldBounds.min.z,
                 worldBounds.max.x, worldBounds.max.y, worldBounds.max.z);

    // Setup plane buffers
    for (auto &plane : planes)
    {
        setupPlaneBuffers(plane, vpos_location, vcol_location, vtex_location);
        spdlog::info("Plane #{} setup", &plane - &planes[0]);
    }

    std::map<const char *, std::pair<GLuint, const char *>> textures = {
        {"resources\\textures\\cube.png", {loadTexture("resources/textures/concrete.jpg", cubeTexture), "Cube Texture"}},
        {"resources\\textures\\plane.png", {loadTexture("resources/textures/grass.jpg", planeTexture), "Plane Texture"}},
    };

    GLFWimage icon;
    icon.pixels = stbi_load("resources\\textures\\icon.png", &icon.width, &icon.height, 0, 4);
    glfwSetWindowIcon(window, 1, &icon);

    initializeTextRenderer(textRenderer);

    double previousTime = glfwGetTime();
    double accumulator = 0.0;
    glm::mat4 model;

    while (!glfwWindowShouldClose(window))
    {
        double deltaTime;
        updateFrameTiming(previousTime, deltaTime, accumulator);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;

        glm::mat4 mvp;
        const int maxUpdates = 3;
        int updates = 0;

        while (accumulator >= fixedDeltaTime && updates < maxUpdates)
        {
            updateCube(window, model, mvp, width, height, ratio, (float)fixedDeltaTime, planes);
            accumulator -= fixedDeltaTime;
            updates++;
        }

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderScene(window, program, mvp_location, cube.VAO, cube.EBO, planes, model, ratio);

        if (showDebugGUI)
        {
            renderImGui();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup(program, cube.VAO, cube.VBO, cube.EBO, planes);
    glDeleteTextures(1, &cubeTexture);
    glDeleteTextures(1, &planeTexture);
    glDeleteProgram(program);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}