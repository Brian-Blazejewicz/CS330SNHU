///////////////////////////////////////////////////////////////////////////////
// SceneManager.cpp
// Render a topiary garden with textured hedges and a spiral centerpiece.
//
// Original framework: Brian Battersby - SNHU
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "stb_image.h"

#include <glm/gtx/transform.hpp>

namespace
{
    const char* g_ModelName = "model";
    const char* g_ColorValueName = "objectColor";
    const char* g_TextureValueName = "objectTexture";
    const char* g_UseTextureName = "bUseTexture";
    const char* g_UseLightingName = "bUseLighting";

    constexpr int MAX_TEXTURES = 16;

    // Custom spiral resources belong to this single scene.
    GLuint g_spiralVAO = 0;
    GLuint g_spiralVBO = 0;
    GLsizei g_spiralVertexCount = 0;

    // Generate a tapered spiral once during scene preparation.
    void CreateSpiralMesh()
    {
        constexpr int segments = 64;
        constexpr int sides = 6;
        constexpr float pi = 3.14159265359f;
        constexpr float turns = 5.0f;
        constexpr float height = 5.0f;

        std::vector<float> vertices;
        vertices.reserve(segments * sides * 6 * 8);

        auto appendVertex = [&](float t, int side)
            {
                const float angle = turns * 2.0f * pi * t;
                const float angularRate = turns * 2.0f * pi;
                const float radius = 1.35f * (1.0f - t) + 0.08f;

                const glm::vec3 centerline(
                    radius * std::cos(angle),
                    0.2f + height * t,
                    radius * std::sin(angle));

                const glm::vec3 tangent = glm::normalize(glm::vec3(
                    -1.35f * std::cos(angle)
                    - radius * angularRate * std::sin(angle),
                    height,
                    -1.35f * std::sin(angle)
                    + radius * angularRate * std::cos(angle)));

                const glm::vec3 radial(
                    std::cos(angle), 0.0f, std::sin(angle));

                const glm::vec3 normalAxis = glm::normalize(
                    radial - tangent * glm::dot(radial, tangent));

                const glm::vec3 binormal = glm::normalize(
                    glm::cross(tangent, normalAxis));

                const float tubeAngle =
                    2.0f * pi * static_cast<float>(side) /
                    static_cast<float>(sides);

                const glm::vec3 normal =
                    normalAxis * std::cos(tubeAngle)
                    + binormal * std::sin(tubeAngle);

                const float tubeRadius =
                    0.26f * (1.0f - t) + 0.07f;

                const glm::vec3 point =
                    centerline + normal * tubeRadius;

                vertices.insert(vertices.end(), {
                    point.x, point.y, point.z,
                    normal.x, normal.y, normal.z,
                    t * turns,
                    static_cast<float>(side) /
                        static_cast<float>(sides)
                    });
            };

        // Six-sided tube: 768 triangles.
        for (int i = 0; i < segments; ++i)
        {
            const float t0 =
                static_cast<float>(i) / segments;

            const float t1 =
                static_cast<float>(i + 1) / segments;

            for (int j = 0; j < sides; ++j)
            {
                appendVertex(t0, j);
                appendVertex(t0, j + 1);
                appendVertex(t1, j);

                appendVertex(t0, j + 1);
                appendVertex(t1, j + 1);
                appendVertex(t1, j);
            }
        }

        g_spiralVertexCount =
            static_cast<GLsizei>(vertices.size() / 8);

        glGenVertexArrays(1, &g_spiralVAO);
        glGenBuffers(1, &g_spiralVBO);

        glBindVertexArray(g_spiralVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_spiralVBO);

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                vertices.size() * sizeof(float)),
            vertices.data(),
            GL_STATIC_DRAW);

        const GLsizei stride =
            static_cast<GLsizei>(8 * sizeof(float));

        // Match the existing shader: position, normal, UV.
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Release the custom mesh before closing the OpenGL context.
    void DestroySpiralMesh()
    {
        if (g_spiralVBO != 0)
        {
            glDeleteBuffers(1, &g_spiralVBO);
            g_spiralVBO = 0;
        }

        if (g_spiralVAO != 0)
        {
            glDeleteVertexArrays(1, &g_spiralVAO);
            g_spiralVAO = 0;
        }

        g_spiralVertexCount = 0;
    }
}

// Initialize mesh and texture storage.
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
    m_pShaderManager = pShaderManager;
    m_basicMeshes = new ShapeMeshes();
    m_loadedTextures = 0;

    for (int i = 0; i < MAX_TEXTURES; ++i)
    {
        m_textureIDs[i].ID = 0;
        m_textureIDs[i].tag.clear();
    }
}

// Release scene resources while the context still exists.
SceneManager::~SceneManager()
{
    DestroySpiralMesh();
    DestroyGLTextures();

    delete m_basicMeshes;
    m_basicMeshes = nullptr;
    m_pShaderManager = nullptr;
}

// Upload an image as an RGBA texture.
bool SceneManager::CreateGLTexture(
    const char* filename, std::string tag)
{
    if (m_loadedTextures >= MAX_TEXTURES)
    {
        std::cerr << "Texture collection is full." << std::endl;
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(
        filename,
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);

    if (image == nullptr)
    {
        std::cerr << "Could not load texture: "
            << filename << std::endl;
        return false;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image);

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textureIDs[m_loadedTextures].ID = textureID;
    m_textureIDs[m_loadedTextures].tag = tag;
    ++m_loadedTextures;

    std::cout << "Loaded texture: " << filename
        << " (" << width << " x " << height << ")"
        << std::endl;

    return true;
}

// Bind textures to their assigned units.
void SceneManager::BindGLTextures()
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
    }

    glActiveTexture(GL_TEXTURE0);
}

// Delete loaded textures.
void SceneManager::DestroyGLTextures()
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        glDeleteTextures(1, &m_textureIDs[i].ID);
        m_textureIDs[i].ID = 0;
        m_textureIDs[i].tag.clear();
    }

    m_loadedTextures = 0;
}

int SceneManager::FindTextureID(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        if (m_textureIDs[i].tag == tag)
        {
            return static_cast<int>(m_textureIDs[i].ID);
        }
    }

    return -1;
}

int SceneManager::FindTextureSlot(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        if (m_textureIDs[i].tag == tag)
        {
            return i;
        }
    }

    return -1;
}

bool SceneManager::FindMaterial(
    std::string tag, OBJECT_MATERIAL& material)
{
    for (const auto& candidate : m_objectMaterials)
    {
        if (candidate.tag == tag)
        {
            material = candidate;
            return true;
        }
    }

    return false;
}

// Build the model matrix for the next object.
void SceneManager::SetTransformations(
    glm::vec3 scaleXYZ,
    float XrotationDegrees,
    float YrotationDegrees,
    float ZrotationDegrees,
    glm::vec3 positionXYZ)
{
    const glm::mat4 scale = glm::scale(scaleXYZ);

    const glm::mat4 rotationX = glm::rotate(
        glm::radians(XrotationDegrees),
        glm::vec3(1.0f, 0.0f, 0.0f));

    const glm::mat4 rotationY = glm::rotate(
        glm::radians(YrotationDegrees),
        glm::vec3(0.0f, 1.0f, 0.0f));

    const glm::mat4 rotationZ = glm::rotate(
        glm::radians(ZrotationDegrees),
        glm::vec3(0.0f, 0.0f, 1.0f));

    const glm::mat4 translation =
        glm::translate(positionXYZ);

    const glm::mat4 model =
        translation * rotationX * rotationY * rotationZ * scale;

    if (m_pShaderManager != nullptr)
    {
        m_pShaderManager->setMat4Value(g_ModelName, model);
    }
}

void SceneManager::SetShaderColor(
    float redColorValue,
    float greenColorValue,
    float blueColorValue,
    float alphaValue)
{
    if (m_pShaderManager == nullptr)
    {
        return;
    }

    m_pShaderManager->setIntValue(g_UseTextureName, false);

    m_pShaderManager->setVec4Value(
        g_ColorValueName,
        glm::vec4(
            redColorValue,
            greenColorValue,
            blueColorValue,
            alphaValue));
}

void SceneManager::SetShaderTexture(std::string textureTag)
{
    if (m_pShaderManager == nullptr)
    {
        return;
    }

    const int slot = FindTextureSlot(textureTag);

    if (slot < 0)
    {
        throw std::runtime_error(
            "Texture tag was not loaded: " + textureTag);
    }

    m_pShaderManager->setIntValue(g_UseTextureName, true);
    m_pShaderManager->setSampler2DValue(
        g_TextureValueName, slot);
}

void SceneManager::SetTextureUVScale(float u, float v)
{
    if (m_pShaderManager != nullptr)
    {
        m_pShaderManager->setVec2Value(
            "UVscale", glm::vec2(u, v));
    }
}

void SceneManager::SetShaderMaterial(std::string materialTag)
{
    if (m_pShaderManager == nullptr)
    {
        return;
    }

    OBJECT_MATERIAL material;

    if (!FindMaterial(materialTag, material))
    {
        throw std::runtime_error(
            "Material tag was not defined: " + materialTag);
    }

    m_pShaderManager->setVec3Value(
        "material.ambientColor", material.ambientColor);

    m_pShaderManager->setFloatValue(
        "material.ambientStrength", material.ambientStrength);

    m_pShaderManager->setVec3Value(
        "material.diffuseColor", material.diffuseColor);

    m_pShaderManager->setVec3Value(
        "material.specularColor", material.specularColor);

    m_pShaderManager->setFloatValue(
        "material.shininess", material.shininess);
}

// Use gravel for ground.png.
void SceneManager::LoadSceneTextures()
{
    if (!CreateGLTexture("textures/bark.png", "bark") ||
        !CreateGLTexture("textures/foliage.png", "foliage") ||
        !CreateGLTexture("textures/ground.png", "ground"))
    {
        throw std::runtime_error(
            "Missing textures. Check the textures folder "
            "and application working directory.");
    }

    BindGLTextures();
}

void SceneManager::DefineObjectMaterials()
{
    m_objectMaterials.clear();

    auto addMaterial = [this](
        const std::string& tag,
        float specularStrength,
        float shininess)
        {
            OBJECT_MATERIAL material;
            material.tag = tag;
            material.ambientStrength = 0.22f;
            material.ambientColor = glm::vec3(1.0f);
            material.diffuseColor = glm::vec3(1.0f);
            material.specularColor = glm::vec3(specularStrength);
            material.shininess = shininess;

            m_objectMaterials.push_back(material);
        };

    addMaterial("ground", 0.08f, 12.0f);
    addMaterial("bark", 0.04f, 8.0f);
    addMaterial("foliage", 0.06f, 12.0f);
}

// Warm main light and cool fill light.
void SceneManager::SetupSceneLights()
{
    if (m_pShaderManager == nullptr)
    {
        return;
    }

    m_pShaderManager->setVec3Value(
        "lights[0].position", glm::vec3(-6.0f, 12.0f, 6.0f));
    m_pShaderManager->setVec3Value(
        "lights[0].color", glm::vec3(1.0f, 0.90f, 0.75f));
    m_pShaderManager->setFloatValue(
        "lights[0].strength", 1.0f);

    m_pShaderManager->setVec3Value(
        "lights[1].position", glm::vec3(7.0f, 10.0f, -8.0f));
    m_pShaderManager->setVec3Value(
        "lights[1].color", glm::vec3(0.70f, 0.82f, 1.0f));
    m_pShaderManager->setFloatValue(
        "lights[1].strength", 0.75f);

    m_pShaderManager->setIntValue(g_UseLightingName, true);
}

// Load reusable geometry once.
void SceneManager::PrepareScene()
{
    LoadSceneTextures();
    DefineObjectMaterials();
    SetupSceneLights();

    m_basicMeshes->LoadCylinderMesh();
    m_basicMeshes->LoadConeMesh();
    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadBoxMesh();

    DestroySpiralMesh();
    CreateSpiralMesh();
}

// Draw a conical topiary with a mostly concealed trunk.
void SceneManager::DrawTree(glm::vec3 position, float size)
{
    SetTransformations(
        glm::vec3(0.25f, 0.8f, 0.25f) * size,
        0.0f, 0.0f, 0.0f,
        position);

    SetShaderTexture("bark");
    SetShaderMaterial("bark");
    SetTextureUVScale(2.0f, 2.0f);
    m_basicMeshes->DrawCylinderMesh();

    // The supplied cone extends from Y = 0 to Y = 1.
    // Place its base close to the ground, like the photograph.
    SetTransformations(
        glm::vec3(1.65f, 5.2f, 1.65f) * size,
        0.0f, 0.0f, 0.0f,
        position + glm::vec3(0.0f, 0.15f * size, 0.0f));

    SetShaderTexture("foliage");
    SetShaderMaterial("foliage");
    SetTextureUVScale(4.0f, 4.0f);
    m_basicMeshes->DrawConeMesh();
}

// Render the garden arrangement.
void SceneManager::RenderScene()
{
    if (m_pShaderManager == nullptr)
    {
        return;
    }

    m_pShaderManager->setIntValue(g_UseLightingName, true);

    // Gravel ground and open path.
    SetTransformations(
        glm::vec3(24.0f, 1.0f, 24.0f),
        0.0f, 0.0f, 0.0f,
        glm::vec3(0.0f, 0.0f, -2.0f));

    SetShaderTexture("ground");
    SetShaderMaterial("ground");
    SetTextureUVScale(12.0f, 12.0f);
    m_basicMeshes->DrawPlaneMesh();

    // Reuse the box mesh to build low clipped hedges.
    auto drawHedge = [this](
        glm::vec3 position, glm::vec3 scale)
        {
            SetTransformations(
                scale,
                0.0f, 0.0f, 0.0f,
                position);

            SetShaderTexture("foliage");
            SetShaderMaterial("foliage");
            SetTextureUVScale(3.0f, 2.0f);
            m_basicMeshes->DrawBoxMesh();
        };

    // Central hedge enclosure.
    drawHedge(
        glm::vec3(0.0f, 0.55f, -3.0f),
        glm::vec3(7.0f, 1.1f, 0.75f));

    drawHedge(
        glm::vec3(-3.15f, 0.55f, -5.0f),
        glm::vec3(0.7f, 1.1f, 4.0f));

    drawHedge(
        glm::vec3(3.15f, 0.55f, -5.0f),
        glm::vec3(0.7f, 1.1f, 4.0f));

    drawHedge(
        glm::vec3(0.0f, 0.55f, -7.0f),
        glm::vec3(7.0f, 1.1f, 0.75f));

    // Hedges along the sides of the path.
    drawHedge(
        glm::vec3(-5.7f, 0.6f, 1.5f),
        glm::vec3(1.0f, 1.2f, 6.0f));

    drawHedge(
        glm::vec3(5.7f, 0.6f, 1.5f),
        glm::vec3(1.0f, 1.2f, 6.0f));

    // Surrounding conical trees.
    DrawTree(glm::vec3(-7.5f, 0.0f, 1.0f), 1.35f);
    DrawTree(glm::vec3(7.5f, 0.0f, 1.0f), 1.55f);

    DrawTree(glm::vec3(-4.8f, 0.0f, -4.8f), 0.80f);
    DrawTree(glm::vec3(4.8f, 0.0f, -5.2f), 0.90f);

    DrawTree(glm::vec3(-3.0f, 0.0f, -9.0f), 0.65f);
    DrawTree(glm::vec3(3.0f, 0.0f, -9.0f), 0.70f);

    // Center topiary: correctly positioned narrow cone core.
    const glm::vec3 topiaryPosition(0.0f, 0.9f, -5.0f);

    SetTransformations(
        glm::vec3(1.05f, 5.4f, 1.05f),
        0.0f, 0.0f, 0.0f,
        topiaryPosition);

    SetShaderTexture("foliage");
    SetShaderMaterial("foliage");
    SetTextureUVScale(3.0f, 4.0f);
    m_basicMeshes->DrawConeMesh();

    // Continuous tapered spiral around the core.
    SetTransformations(
        glm::vec3(1.0f),
        0.0f, 0.0f, 0.0f,
        topiaryPosition);

    SetShaderTexture("foliage");
    SetShaderMaterial("foliage");
    SetTextureUVScale(2.0f, 1.0f);

    glBindVertexArray(g_spiralVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_spiralVertexCount);
    glBindVertexArray(0);
}