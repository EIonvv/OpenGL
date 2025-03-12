#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <iostream>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include "../config.h"

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Static string to store combined keys during a press sequence
    static std::string pressedKeys;
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        spdlog::info("Escape key pressed. Closing the window...");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        pressedKeys.clear(); // Reset on escape
    }

    if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
    {
        if (mode == debug)
        {
            mode = release;
            spdlog::info("Switching to release mode");
        }
        else if (mode == release)
        {
            mode = debug;
            spdlog::info("Switching to debug mode");
        }
        pressedKeys.clear(); // Reset on mode switch
    }

    const char* actionStr = (action == GLFW_PRESS) ? "pressed" : 
                           (action == GLFW_RELEASE) ? "released" : 
                           "repeated";

    // Convert key code to character where possible
    char keyChar = '\0';
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
    {
        keyChar = 'A' + (key - GLFW_KEY_A) + (mods & GLFW_MOD_SHIFT ? 0 : 32); // Uppercase or lowercase
    }
    else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
    {
        keyChar = '0' + (key - GLFW_KEY_0);
    }
    // Add more special characters if needed
    else if (key == GLFW_KEY_SPACE) keyChar = ' ';
    else if (key == GLFW_KEY_ENTER) keyChar = '\n';
    // Could add more mappings for symbols like GLFW_KEY_COMMA, GLFW_KEY_PERIOD, etc.

    if (action == GLFW_PRESS && keyChar != '\0')
    {
        if (!pressedKeys.empty())
            pressedKeys += " + ";
        pressedKeys += keyChar;
    }
    
    if (mode == debug)
    {
        if (keyChar != '\0')
        {
            if (action == GLFW_RELEASE && !pressedKeys.empty())
            {
                spdlog::info("Keys {} ({}) {} - Combined: {}", 
                    keyChar, scancode, actionStr, pressedKeys);
                pressedKeys.clear(); // Reset after logging release
            }
            else if (action == GLFW_PRESS)
            {
                spdlog::info("Key {} ({}) {} - Combined: {}", 
                    keyChar, scancode, actionStr, pressedKeys);
            }
        }
        else
        {
            spdlog::info("Key {} ({}) {}", key, scancode, actionStr);
        }
    }
    else if (mode == release)
    {
        if (keyChar != '\0')
        {
            if (action == GLFW_RELEASE && !pressedKeys.empty())
            {
                spdlog::info("Keys {} {} - Combined: {}", 
                    keyChar, actionStr, pressedKeys);
                pressedKeys.clear(); // Reset after logging release
            }
            else if (action == GLFW_PRESS)
            {
                spdlog::info("Key {} {} - Combined: {}", 
                    keyChar, actionStr, pressedKeys);
            }
        }
        else
        {
            // spdlog::info("Key {} {}", key, actionStr);
            if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
            {
                spdlog::info("Control key {} {}", actionStr, pressedKeys);
            }
            else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
            {
                spdlog::info("Shift key {} {}", actionStr, pressedKeys);
            }
            else if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT)
            {
                spdlog::info("Alt key {} {}", actionStr, pressedKeys);
            }
            else
            {
                spdlog::info("Key {} {}", key, actionStr);
            }
        }
    }
}

#endif /* KEYBOARD_H */
