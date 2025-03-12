#ifndef GETMOUSE_H
#define GETMOUSE_H

#include <iostream>
#include <windows.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

// Custom namespace or function for mouse handling (placeholder)
class GetMouse
{
public:
    static glm::vec2 getMousePosition(GLFWwindow *window)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
    }
};

#endif /* GETMOUSE_H */
