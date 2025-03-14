#ifndef CONFIG_H
#define CONFIG_H

#define debug 1
#define release 0
int mode = release;

static bool isDragging = false;
static int targetFPS = 144;
static double targetFrameTime = 1.0 / targetFPS;

static glm::vec3 SquarePos(0.0f);
static glm::vec2 lastMousePos(0.0f);
static double lastTime = 0.0;
static int frameCount = 0;
static double currentFPS = 0.0;
static TextRenderer *textRenderer = nullptr;
static glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
static bool cubePOVMode = false;

// Collision detection
static bool isColliding = false;
static int collidingPlaneIndex = -1;
const double fixedDeltaTime = 1.0 / 60.0f;

// wondering globals
glm::vec3 wanderDirection = glm::vec3(1.0f, 0.0f, 0.0f); // Initial wander direction
float wanderTimer = 0.0f; // Timer to change direction
float wanderChangeInterval = 1.0f; // Change direction every 2 seconds

#endif /* CONFIG_H */
