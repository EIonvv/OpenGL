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

#endif /* MOUSE_CALLBACK_H */
