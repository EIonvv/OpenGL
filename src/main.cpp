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

    vertices[0] = {{position.x + bottomLeft.x, position.y + bottomLeft.y, position.z + bottomLeft.z}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}};    // Bottom-left
    vertices[1] = {{position.x + bottomRight.x, position.y + bottomRight.y, position.z + bottomRight.z}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}}; // Bottom-right
    vertices[2] = {{position.x + topRight.x, position.y + topRight.y, position.z + topRight.z}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}};          // Top-right
    vertices[3] = {{position.x + topLeft.x, position.y + topLeft.y, position.z + topLeft.z}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}};             // Top-left

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

    // Default gravity direction (downward along Y-axis if no collision)
    glm::vec3 gravityDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    float gravityStrength = 9.8f; // Gravity acceleration (m/s^2), adjust as needed

    // Check for collision and adjust gravity direction if colliding
    if (isCubeCollidingWithPlane(model, planes, collidingPlaneIndex))
    {
        const Plane &collidingPlane = planes[collidingPlaneIndex];

        // Calculate plane normal (using the snippet's method)
        glm::vec3 edge1 = glm::make_vec3(collidingPlane.vertices[1].pos) - glm::make_vec3(collidingPlane.vertices[0].pos);
        glm::vec3 edge2 = glm::make_vec3(collidingPlane.vertices[2].pos) - glm::make_vec3(collidingPlane.vertices[0].pos);
        glm::vec3 planeNormal = glm::normalize(glm::cross(edge1, edge2));

        // Gravity acts opposite to the plane normal (downward relative to the plane)
        gravityDirection = -planeNormal;

        // Calculate penetration depth and resolve collision
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
            if (distance < 0.0f) // Vertex is below the plane
            {
                minPenetration = std::min(minPenetration, std::abs(distance));
            }
        }

        // Push the cube up along the plane normal by the penetration depth
        if (minPenetration != std::numeric_limits<float>::max())
        {
            SquarePos += planeNormal * minPenetration * (float)deltaTime * 0.01f; // Scale for reasonable speed
            model = glm::translate(glm::mat4(1.0f), SquarePos);                   // Update model matrix
        }
    }

    // Apply gravity
    SquarePos += gravityDirection * gravityStrength * deltaTime * 0.01f; // Scale for reasonable speed
    model = glm::translate(glm::mat4(1.0f), SquarePos);                  // Update model matrix after gravity
}

// Update cube
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

    if (deltaTime > 0.1f)
    {
        speed = baseSpeed * 0.1f;
    }

    // Existing movement controls
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
    
    // Wandering behavior when not in cubePOVMode and colliding with a plane
    if (!cubePOVMode && isCubeCollidingWithPlane(model, planes, collidingPlaneIndex))
    {
        // Update wander timer
        wanderTimer += deltaTime;
        if (wanderTimer >= wanderChangeInterval)
        {
            // Randomize a new direction (on the XZ plane for flat movement)
            float randomAngle = static_cast<float>(rand()) / RAND_MAX * 360.0f; // Random angle in degrees
            wanderDirection = glm::normalize(glm::vec3(cos(glm::radians(randomAngle)), 0.0f, sin(glm::radians(randomAngle))));
            wanderTimer = 0.0f; // Reset timer
        }

        // Move the cube in the wander direction
        float wanderSpeed = 2.0f; // Adjust speed as needed
        glm::vec3 newPos = SquarePos + wanderDirection * wanderSpeed * deltaTime;

        // Ensure the cube stays within plane bounds (simple boundary check)
        for (const auto &plane : planes)
        {
            // Calculate the vertices of the plane
            glm::vec3 bottomLeft = glm::make_vec3(plane.vertices[0].pos);
            glm::vec3 bottomRight = glm::make_vec3(plane.vertices[1].pos);
            glm::vec3 topRight = glm::make_vec3(plane.vertices[2].pos);
            glm::vec3 topLeft = glm::make_vec3(plane.vertices[3].pos);


            // Check if the new position is within the plane bounds
            if (newPos.x >= bottomLeft.x && newPos.x <= bottomRight.x &&
                newPos.z >= bottomLeft.z && newPos.z <= topLeft.z)
            {
                SquarePos = newPos; // Update position
                break;
            }

            
        }

        SquarePos = newPos; // Update position
    }

    // Existing dragging logic
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

    glfwSetErrorCallback([](int error, const char *description)
                         { spdlog::error("GLFW Error {}: {}", error, description); });

    if (!glfwInit())
    {
        spdlog::critical("Failed to initialize GLFW. Terminating program.");
        glfwTerminate();    // Ensure GLFW is terminated even on failure
        exit(EXIT_FAILURE); // Exit with failure code
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
        glfwDestroyWindow(window); // Clean up window before terminating
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);

    spdlog::info("GLFW and OpenGL initialized successfully.");

    return window;
}

// Main function
int main(int argc, char *argv[])
{
    // Parse command-line arguments
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

    // Initialize GLFW and window
    GLFWwindow *window = initializeWindow();

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    if (io.Fonts->AddFontFromFileTTF("resources\\fonts\\arlrbd.ttf", 16.0f) == NULL)
    {
        spdlog::error("Failed to load font file");
        return -1;
    }

    // Setup ImGui style    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

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

    // Generate planes  
    std::vector<Plane> planes;

    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, 0.0f, 0.0f)}); // Ground plane

    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(0.0f, 0.0f, 12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});    // Back plane
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(12.0f, 0.0f, 12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});   // back right
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(12.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});    // Right plane
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(12.0f, 0.0f, -12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});  // front right
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(0.0f, 0.0f, -12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});   // Front plane
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(-12.0f, 0.0f, -12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)}); // front left
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(-12.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});   // Left plane
    planes.push_back({generatePlaneVertices(12.0f, 12.0f, glm::vec3(-12.0f, 0.0f, 12.0f), glm::vec3(0.0f, 0.0f, 0.0f)), 0, 0, 0, glm::vec3(0.0f, planes[0].position.y - 1.0f, 0.0f)});  // back left

    // Setup plane buffers
    for (auto &plane : planes)
    {
        setupPlaneBuffers(plane, vpos_location, vcol_location, vtex_location);
    }

    // Load textures and make a map for the textures
    std::map<const char *, std::pair<GLuint, const char *>> textures = {
        {"resources\\textures\\cube.png", {loadTexture("resources/textures/concrete.jpg", cubeTexture), "Cube Texture"}},
        {"resources\\textures\\plane.png", {loadTexture("resources/textures/grass.jpg", planeTexture), "Plane Texture"}},
    };

    // Load icon
    GLFWimage icon;
    icon.pixels = stbi_load("resources\\textures\\icon.png", &icon.width, &icon.height, 0, 4);
    glfwSetWindowIcon(window, 1, &icon);

    // Initialize text renderer
    initializeTextRenderer(textRenderer);

    double previousTime = glfwGetTime();
    glm::mat4 model;
    double accumulator = 0.0;

    while (!glfwWindowShouldClose(window))
    {

        // Frame timing
        double deltaTime;
        updateFrameTiming(previousTime, deltaTime, accumulator);

        // Get window size
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;

        // Update cube
        glm::mat4 mvp;
        const int maxUpdates = 5; // Prevent spiral of death
        int updates = 0;

        // Update cube position and orientations
        while (accumulator >= fixedDeltaTime && updates < maxUpdates)
        {
            updateCube(window, model, mvp, width, height, ratio, (float)fixedDeltaTime, planes);
            accumulator -= fixedDeltaTime;
            updates++;
        }

        // Clear the screen
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render 3D scene
        renderScene(window, program, mvp_location, cubeVAO, cubeEBO, planes, model, ratio);

        // Render ImGui
        if (showDebugGUI)
        {
            renderImGui();
        }

        glEnable(GL_DEPTH_TEST); // Re-enable depth test for next frame

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();

    }

    // Cleanup
    cleanup(program, cubeVAO, cubeVBO, cubeEBO, planes);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (window)
    {
        glfwDestroyWindow(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}