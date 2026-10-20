///////////////////////////////////////////////////////////////////////////////
// ViewManager.cpp
// Manage camera movement, mouse input, and scene projection.
//
// Original framework: Brian Battersby, SNHU
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr int WINDOW_WIDTH = 1000;
    constexpr int WINDOW_HEIGHT = 800;

    Camera* g_pCamera = nullptr;

    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;

    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;
    bool gFirstFrame = true;

    float gMovementSpeed = 5.0f;

    float gYaw = -90.0f;
    float gPitch = -14.0f;

    bool gOrthographicProjection = false;

    // Derive the viewing direction from yaw and pitch.
    glm::vec3 CalculateCameraDirection()
    {
        const float yawRadians = glm::radians(gYaw);
        const float pitchRadians = glm::radians(gPitch);

        return glm::normalize(glm::vec3(
            std::cos(yawRadians) * std::cos(pitchRadians),
            std::sin(pitchRadians),
            std::sin(yawRadians) * std::cos(pitchRadians)));
    }
}

// Start closer to the garden with a narrower field of view.
ViewManager::ViewManager(ShaderManager* pShaderManager)
{
    m_pShaderManager = pShaderManager;
    m_pWindow = nullptr;

    gLastX = WINDOW_WIDTH / 2.0f;
    gLastY = WINDOW_HEIGHT / 2.0f;
    gFirstMouse = true;

    gDeltaTime = 0.0f;
    gLastFrame = 0.0f;
    gFirstFrame = true;

    gMovementSpeed = 5.0f;
    gYaw = -90.0f;
    gPitch = -8.0f;
    gOrthographicProjection = false;

    g_pCamera = new Camera();
    g_pCamera->Position = glm::vec3(0.0f, 3.4f, 11.0f);
    g_pCamera->Front = CalculateCameraDirection();
    g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
    g_pCamera->Zoom = 55.0f;
}

// Release the camera. MainCode.cpp owns the GLFW window.
ViewManager::~ViewManager()
{
    delete g_pCamera;
    g_pCamera = nullptr;

    m_pWindow = nullptr;
    m_pShaderManager = nullptr;
}

// Create the window and register mouse callbacks.
GLFWwindow* ViewManager::CreateDisplayWindow(
    const char* windowTitle)
{
    GLFWwindow* window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        windowTitle,
        nullptr,
        nullptr);

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window."
            << std::endl;
        return nullptr;
    }

    glfwMakeContextCurrent(window);

    glfwSetInputMode(
        window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetCursorPosCallback(
        window, &ViewManager::Mouse_Position_Callback);

    glfwSetScrollCallback(
        window, &ViewManager::Mouse_Scroll_Callback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_pWindow = window;
    return window;
}

// Rotate the viewing direction without moving the camera.
void ViewManager::Mouse_Position_Callback(
    GLFWwindow* window,
    double xMousePos,
    double yMousePos)
{
    if (g_pCamera == nullptr)
    {
        return;
    }

    if (gFirstMouse)
    {
        gLastX = static_cast<float>(xMousePos);
        gLastY = static_cast<float>(yMousePos);
        gFirstMouse = false;
        return;
    }

    constexpr float sensitivity = 0.10f;

    const float xOffset =
        static_cast<float>(xMousePos) - gLastX;

    const float yOffset =
        gLastY - static_cast<float>(yMousePos);

    gLastX = static_cast<float>(xMousePos);
    gLastY = static_cast<float>(yMousePos);

    gYaw += xOffset * sensitivity;
    gPitch += yOffset * sensitivity;

    // Prevent the viewing direction from becoming parallel to world up.
    gPitch = std::max(-89.0f, std::min(89.0f, gPitch));

    g_pCamera->Front = CalculateCameraDirection();
}

// Adjust travel speed with the mouse wheel.
void ViewManager::Mouse_Scroll_Callback(
    GLFWwindow* window,
    double xOffset,
    double yOffset)
{
    gMovementSpeed = std::max(
        0.5f,
        std::min(
            20.0f,
            gMovementSpeed + static_cast<float>(yOffset)));
}

// Move the camera and select the projection mode.
void ViewManager::ProcessKeyboardEvents()
{
    if (m_pWindow == nullptr || g_pCamera == nullptr)
    {
        return;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_pWindow, true);
    }

    const float distance = gMovementSpeed * gDeltaTime;

    const glm::vec3 right = glm::normalize(
        glm::cross(g_pCamera->Front, g_pCamera->Up));

    if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        g_pCamera->Position += g_pCamera->Front * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        g_pCamera->Position -= g_pCamera->Front * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        g_pCamera->Position -= right * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        g_pCamera->Position += right * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
    {
        g_pCamera->Position += g_pCamera->Up * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
        g_pCamera->Position -= g_pCamera->Up * distance;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS)
    {
        gOrthographicProjection = false;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS)
    {
        gOrthographicProjection = true;
    }
}

// Update movement, viewport dimensions, and shader matrices.
void ViewManager::PrepareSceneView()
{
    if (m_pWindow == nullptr || g_pCamera == nullptr)
    {
        return;
    }

    const float currentFrame =
        static_cast<float>(glfwGetTime());

    if (gFirstFrame)
    {
        gDeltaTime = 0.0f;
        gFirstFrame = false;
    }
    else
    {
        // Limit sudden movement after a long pause or frame delay.
        gDeltaTime = std::max(
            0.0f,
            std::min(currentFrame - gLastFrame, 0.1f));
    }

    gLastFrame = currentFrame;

    ProcessKeyboardEvents();

    int width = 1;
    int height = 1;

    glfwGetFramebufferSize(m_pWindow, &width, &height);

    width = std::max(width, 1);
    height = std::max(height, 1);

    glViewport(0, 0, width, height);

    const float aspect =
        static_cast<float>(width) /
        static_cast<float>(height);

    // Both projections share this camera position and orientation.
    const glm::mat4 view = glm::lookAt(
        g_pCamera->Position,
        g_pCamera->Position + g_pCamera->Front,
        g_pCamera->Up);

    glm::mat4 projection;

    if (gOrthographicProjection)
    {
        constexpr float halfHeight = 7.0f;

        projection = glm::ortho(
            -halfHeight * aspect,
            halfHeight * aspect,
            -halfHeight,
            halfHeight,
            0.1f,
            100.0f);
    }
    else
    {
        projection = glm::perspective(
            glm::radians(g_pCamera->Zoom),
            aspect,
            0.1f,
            100.0f);
    }

    if (m_pShaderManager != nullptr)
    {
        m_pShaderManager->setMat4Value("view", view);
        m_pShaderManager->setMat4Value(
            "projection", projection);

        m_pShaderManager->setVec3Value(
            "viewPosition", g_pCamera->Position);
    }
}