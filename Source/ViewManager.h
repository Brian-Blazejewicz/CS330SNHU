///////////////////////////////////////////////////////////////////////////////
// ViewManager.h
// =============
// Manage the viewing of 3D objects within the viewport.
//
// Original framework: Brian Battersby, SNHU
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "camera.h"
#include "GLFW/glfw3.h"

class ViewManager
{
public:
    ViewManager(ShaderManager* pShaderManager);
    ~ViewManager();

    static void Mouse_Position_Callback(
        GLFWwindow* window,
        double xMousePos,
        double yMousePos);

    static void Mouse_Scroll_Callback(
        GLFWwindow* window,
        double xOffset,
        double yOffset);

    GLFWwindow* CreateDisplayWindow(const char* windowTitle);
    void PrepareSceneView();

private:
    ShaderManager* m_pShaderManager;
    GLFWwindow* m_pWindow;

    void ProcessKeyboardEvents();
};