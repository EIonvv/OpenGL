#ifndef GET_MOUSE_POSITION_H
#define GET_MOUSE_POSITION_H


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


#endif /* GET_MOUSE_POSITION_H */
