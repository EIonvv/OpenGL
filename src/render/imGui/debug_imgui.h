#ifndef CUSTOM_IMGUI_H
#define CUSTOM_IMGUI_H

#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <gl/GL.h>
#include "../../globals.h"

static void renderImGui()
{
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Calculate FPS loss (difference between targetFPS and currentFPS, if current is lower)
    float fpsLoss = (currentFPS < targetFPS) ? static_cast<float>(targetFPS - currentFPS) : 0.0f;

    // clang-format off
    // ImGui window
    ImGui::Begin("Debug Window");
    ImGui::Text("FPS:"); 
    ImGui::SameLine(); 
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%.1f", currentFPS);
    ImGui::SameLine(); 
    ImGui::Text("( Loss:"); 
    ImGui::SameLine(); 
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%.1f", fpsLoss); 
    ImGui::SameLine(); 
    ImGui::Text(")");

    ImGui::Text("OpenGL:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", glGetString(GL_VERSION));
    ImGui::Text("GPU:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.0f, 0.0f, 1.0f, 1.0f), "%s", glGetString(GL_RENDERER));

    ImGui::Separator();

    ImGui::Text("Cube Rotation:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(%.1f, %.1f)", rotationAngles.x, rotationAngles.y);
    ImGui::Text("Cube Position:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(%.2f, %.2f, %.2f)", SquarePos.x, SquarePos.y, SquarePos.z);
    ImGui::Text("Camera Position:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.5f, 1.0f), "(%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
    
    // ImGui::Text("Box Collision:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s with plane %d", isColliding ? "Colliding" : "Not colliding", collidingPlaneIndex);
    // ImGui::Text("Total Planes:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", planes.size());
    // Display the amount of Planes and the current Plane index
    // Draw a line to separate the text
    ImGui::Separator();
    ImGui::Text("Total Planes:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", planes.size());
    ImGui::Text("Current Plane:"); ImGui::SameLine(); ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", collidingPlaneIndex);
    // clang-format on

    // Set button style for dark mode
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));        // Dark gray color
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Lighter gray when hovered
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));  // Darker gray when active
    if (ImGui::Button("Toggle Cube POV Mode"))
    {
        cubePOVMode = !cubePOVMode;
    }

    ImGui::PopStyleColor(3); // Pop the three style colors

    ImGui::End();

    // Render ImGui
    glDisable(GL_DEPTH_TEST); // Disable depth test for ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif /* CUSTOM_IMGUI_H */