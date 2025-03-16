#ifndef GLOBALS_H
#define GLOBALS_H

#define debug 1
#define release 0

#include "render/scene/structures/plane/plane_struct.h"
#include "render/scene/structures/cube/cube_struct.h"

// debug/release mode
static int mode = release;

// Mouse input
static int targetFPS = 144;
static double targetFrameTime = 1.0 / targetFPS;

// Cube movement
extern bool cubePOVMode; // Add this to access cubePOVMode from main.cpp
static glm::vec3 lastSquarePos(0.0f);
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
const float bufferZone = 0.5f;                           // Buffer zone to avoid getting too close to boundaries
const float BOOST_DURATION_DISTANCE = 10.0f; // Distance in units the boost lasts
const float BOOST_MULTIPLIER = 2.5f;         // Speed multiplier during boost
const float turnSpeed = 2.0f;

glm::vec3 wanderDirection = glm::vec3(1.0f, 0.0f, 0.0f); // Initial wander direction
float wanderTimer = 0.0f;                                // Timer to change direction
float wanderChangeInterval = 2.0f;                       // Change direction every 2 seconds
int lastCollidingPlaneIndex = 0;                         // Index of the last plane the cube was on
bool isBoostActive = false;
float boostDistanceTraveled = 0.0f;
float wanderSpeed = 5.0f;

// object globals
static std::vector<Plane> planes;
static std::vector<Cube> cubes;

#endif /* GLOBALS_H */
