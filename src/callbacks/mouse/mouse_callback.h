#ifndef MOUSE_CALLBACK_H
#define MOUSE_CALLBACK_H

#include <iostream>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "../../config.h"

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            isDragging = true;
            if (mode == debug)
            {
                spdlog::info("{}: Mouse button pressed", __func__);
            }
        }
        else if (action == GLFW_RELEASE)
        {
            if (mode == debug)
            {
                spdlog::info("{}: Mouse button released", __func__);
            }
            isDragging = false;
        }
    }
}

// Mouse callback for camera rotation
static glm::vec2 rotationAngles(0.0f, 0.0f);
static glm::vec2 mouseDelta(0.0f);

static void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    static bool firstMouse = true;
    static glm::vec2 lastPos(0.0f);

    if (firstMouse)
    {
        lastPos = glm::vec2(xpos, ypos);
        firstMouse = false;
    }
    
    glm::vec2 currentMousePos(xpos, ypos);
    if (lastPos != glm::vec2(0.0f)) // Skip first frame
    {
        mouseDelta += currentMousePos - lastPos;
    }
    lastPos = currentMousePos;

    glm::vec2 currentPos(xpos, ypos);
    glm::vec2 delta = currentPos - lastPos;

    // Sensitivity
    float sensitivity = 0.1f;
    delta *= sensitivity;

    // Update rotation angles (yaw and pitch)
    rotationAngles.y += delta.x;                                    // Yaw (left/right)
    rotationAngles.x -= delta.y;                                    // Pitch (up/down)
    rotationAngles.x = glm::clamp(rotationAngles.x, -89.0f, 89.0f); // Limit pitch to avoid flipping

    lastPos = currentPos;

    // Capture mouse for continuous movement
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

#endif /* MOUSE_CALLBACK_H */
