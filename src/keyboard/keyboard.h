#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <iostream>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>
#include <unordered_map>

#include "../config.h"

// Key state tracking structure
struct KeyState {
    static std::string pressedKeys;
    static std::unordered_map<int, bool> keyStates;
};

// Initialize static members
std::string KeyState::pressedKeys;
std::unordered_map<int, bool> KeyState::keyStates;

// Comprehensive key to string mapping
static const std::unordered_map<int, std::pair<std::string, std::string>> keyMap = {
    // Letters (lowercase, uppercase)
    {GLFW_KEY_A, {"a", "A"}}, {GLFW_KEY_B, {"b", "B"}}, {GLFW_KEY_C, {"c", "C"}},
    {GLFW_KEY_D, {"d", "D"}}, {GLFW_KEY_E, {"e", "E"}}, {GLFW_KEY_F, {"f", "F"}},
    {GLFW_KEY_G, {"g", "G"}}, {GLFW_KEY_H, {"h", "H"}}, {GLFW_KEY_I, {"i", "I"}},
    {GLFW_KEY_J, {"j", "J"}}, {GLFW_KEY_K, {"k", "K"}}, {GLFW_KEY_L, {"l", "L"}},
    {GLFW_KEY_M, {"m", "M"}}, {GLFW_KEY_N, {"n", "N"}}, {GLFW_KEY_O, {"o", "O"}},
    {GLFW_KEY_P, {"p", "P"}}, {GLFW_KEY_Q, {"q", "Q"}}, {GLFW_KEY_R, {"r", "R"}},
    {GLFW_KEY_S, {"s", "S"}}, {GLFW_KEY_T, {"t", "T"}}, {GLFW_KEY_U, {"u", "U"}},
    {GLFW_KEY_V, {"v", "V"}}, {GLFW_KEY_W, {"w", "W"}}, {GLFW_KEY_X, {"x", "X"}},
    {GLFW_KEY_Y, {"y", "Y"}}, {GLFW_KEY_Z, {"z", "Z"}},
    
    // Numbers
    {GLFW_KEY_0, {"0", ")"}}, {GLFW_KEY_1, {"1", "!"}}, {GLFW_KEY_2, {"2", "@"}},
    {GLFW_KEY_3, {"3", "#"}}, {GLFW_KEY_4, {"4", "$"}}, {GLFW_KEY_5, {"5", "%"}},
    {GLFW_KEY_6, {"6", "^"}}, {GLFW_KEY_7, {"7", "&"}}, {GLFW_KEY_8, {"8", "*"}},
    {GLFW_KEY_9, {"9", "("}},
    
    // Symbols
    {GLFW_KEY_SPACE, {" ", " "}}, {GLFW_KEY_ENTER, {"[ENTER]", "[ENTER]"}},
    {GLFW_KEY_TAB, {"[TAB]", "[TAB]"}}, {GLFW_KEY_BACKSPACE, {"[BACKSPACE]", "[BACKSPACE]"}},
    {GLFW_KEY_COMMA, {",", "<"}}, {GLFW_KEY_PERIOD, {".", ">"}},
    {GLFW_KEY_SLASH, {"/", "?"}}, {GLFW_KEY_SEMICOLON, {";", ":"}},
    {GLFW_KEY_APOSTROPHE, {"'", "\""}}, {GLFW_KEY_LEFT_BRACKET, {"[", "{"}},
    {GLFW_KEY_RIGHT_BRACKET, {"]", "}"}}, {GLFW_KEY_BACKSLASH, {"\\", "|"}},
    {GLFW_KEY_MINUS, {"-", "_"}}, {GLFW_KEY_EQUAL, {"=", "+"}},
    {GLFW_KEY_GRAVE_ACCENT, {"`", "~"}},
    
    // Function keys
    {GLFW_KEY_F1, {"[F1]", "[F1]"}}, {GLFW_KEY_F2, {"[F2]", "[F2]"}},
    {GLFW_KEY_F3, {"[F3]", "[F3]"}}, {GLFW_KEY_F4, {"[F4]", "[F4]"}},
    
    // Modifier keys
    {GLFW_KEY_LEFT_SHIFT, {"[L-SHIFT]", "[L-SHIFT]"}},
    {GLFW_KEY_RIGHT_SHIFT, {"[R-SHIFT]", "[R-SHIFT]"}},
    {GLFW_KEY_LEFT_CONTROL, {"[L-CTRL]", "[L-CTRL]"}},
    {GLFW_KEY_RIGHT_CONTROL, {"[R-CTRL]", "[R-CTRL]"}},
    {GLFW_KEY_LEFT_ALT, {"[L-ALT]", "[L-ALT]"}},
    {GLFW_KEY_RIGHT_ALT, {"[R-ALT]", "[R-ALT]"}},

    // Arrow keys
    {GLFW_KEY_UP, {"[UP]", "[UP]"}}, {GLFW_KEY_DOWN, {"[DOWN]", "[DOWN]"}},
    {GLFW_KEY_LEFT, {"[LEFT]", "[LEFT]"}}, {GLFW_KEY_RIGHT, {"[RIGHT]", "[RIGHT]"}}
    
};

// key_callback remains unchanged
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const char* actionStr = (action == GLFW_PRESS) ? "pressed" :
                           (action == GLFW_RELEASE) ? "released" :
                           "repeated";

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        spdlog::info("Escape key pressed. Closing the window...");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        KeyState::pressedKeys.clear();
        KeyState::keyStates.clear();
        return;
    }

    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        mode = (mode == debug) ? release : debug;
        spdlog::info("Switching to {} mode", (mode == debug) ? "debug" : "release");
        // display the console in debug mode
        if (mode == debug) {
            FreeConsole();
            AllocConsole();
            FILE* file = nullptr;
            freopen_s(&file, "CONOUT$", "w", stdout);
            SetConsoleTitle("Debug Console");
            spdlog::info("Debug console initialized");
        }
        else if (mode == release) {
            // close the console
            FreeConsole();
        }
        KeyState::pressedKeys.clear();
        KeyState::keyStates.clear();
        return;
    }

    KeyState::keyStates[key] = (action != GLFW_RELEASE);

    std::string keyStr = std::to_string(key);
    auto it = keyMap.find(key);
    if (it != keyMap.end()) {
        bool isShift = mods & GLFW_MOD_SHIFT;
        keyStr = isShift ? it->second.second : it->second.first;
    }

    if (action == GLFW_PRESS) {
        if (!KeyState::pressedKeys.empty()) {
            KeyState::pressedKeys += " + ";
        }
        KeyState::pressedKeys += keyStr;
    }

    if (mode == debug) {
        spdlog::info("Key {} ({}) {} - Mods: {}{}{}{} - Combined: {}", 
            keyStr, scancode, actionStr,
            (mods & GLFW_MOD_SHIFT) ? "SHIFT " : "",
            (mods & GLFW_MOD_CONTROL) ? "CTRL " : "",
            (mods & GLFW_MOD_ALT) ? "ALT " : "",
            (mods & GLFW_MOD_SUPER) ? "SUPER " : "",
            KeyState::pressedKeys);
        
        if (action == GLFW_RELEASE) {
            KeyState::pressedKeys.clear();
        }
    }
    else if (mode == release) {
        spdlog::info("Key {} {} - Combined: {}", 
            keyStr, actionStr, KeyState::pressedKeys);
        
        if (action == GLFW_RELEASE) {
            KeyState::pressedKeys.clear();
        }
    }
}

#endif /* KEYBOARD_H */