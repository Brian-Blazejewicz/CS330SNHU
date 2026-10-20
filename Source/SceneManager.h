///////////////////////////////////////////////////////////////////////////////
// SceneManager.h
// Manage textures, materials, lighting, and landscape rendering.
// Original framework: Brian Battersby - SNHU
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "ShaderManager.h"
#include "ShapeMeshes.h"

#include <cstdint>
#include <string>
#include <vector>

class SceneManager
{
public:
    SceneManager(ShaderManager* pShaderManager);
    ~SceneManager();

    struct TEXTURE_INFO
    {
        std::string tag;
        std::uint32_t ID = 0;
    };

    struct OBJECT_MATERIAL
    {
        float ambientStrength = 0.0f;
        glm::vec3 ambientColor = glm::vec3(0.0f);
        glm::vec3 diffuseColor = glm::vec3(0.0f);
        glm::vec3 specularColor = glm::vec3(0.0f);
        float shininess = 0.0f;
        std::string tag;
    };

    void LoadSceneTextures();
    void DefineObjectMaterials();
    void SetupSceneLights();
    void PrepareScene();
    void RenderScene();

private:
    ShaderManager* m_pShaderManager = nullptr;
    ShapeMeshes* m_basicMeshes = nullptr;

    int m_loadedTextures = 0;
    TEXTURE_INFO m_textureIDs[16];

    std::vector<OBJECT_MATERIAL> m_objectMaterials;

    // Texture loading and cleanup.
    bool CreateGLTexture(
        const char* filename,
        std::string tag);

    void BindGLTextures();
    void DestroyGLTextures();

    int FindTextureID(std::string tag);
    int FindTextureSlot(std::string tag);

    bool FindMaterial(
        std::string tag,
        OBJECT_MATERIAL& material);

    // Transformation and appearance settings.
    void SetTransformations(
        glm::vec3 scaleXYZ,
        float XrotationDegrees,
        float YrotationDegrees,
        float ZrotationDegrees,
        glm::vec3 positionXYZ);

    void SetShaderColor(
        float redColorValue,
        float greenColorValue,
        float blueColorValue,
        float alphaValue);

    void SetShaderTexture(std::string textureTag);
    void SetTextureUVScale(float u, float v);
    void SetShaderMaterial(std::string materialTag);

    // Build a reusable tree from a trunk and canopy.
    void DrawTree(glm::vec3 position, float size);
};