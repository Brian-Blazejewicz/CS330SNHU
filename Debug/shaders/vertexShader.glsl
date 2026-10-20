#version 330 core
// SNHU ShapeMeshes attributes: position, normal, texture coordinates.
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 textureCoordinate;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragmentPosition;
out vec3 surfaceNormal;
out vec2 textureUV;

void main()
{
    vec4 worldPosition = model * vec4(position, 1.0);
    fragmentPosition = worldPosition.xyz;
    // Inverse transpose keeps normals correct for nonuniform scaling.
    surfaceNormal = mat3(transpose(inverse(model))) * normal;
    textureUV = textureCoordinate;
    gl_Position = projection * view * worldPosition;
}
