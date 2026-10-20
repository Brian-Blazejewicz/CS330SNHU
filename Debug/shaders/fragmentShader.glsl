#version 330 core
in vec3 fragmentPosition;
in vec3 surfaceNormal;
in vec2 textureUV;
out vec4 fragmentColor;

struct Material
{
    vec3 ambientColor;
    float ambientStrength;
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
};
struct PointLight
{
    vec3 position;
    vec3 color;
    float strength;
};
uniform Material material;
uniform PointLight lights[2];
uniform vec3 viewPosition;
uniform sampler2D objectTexture;
uniform vec4 objectColor;
uniform vec2 UVscale;
uniform bool bUseTexture;
uniform bool bUseLighting;

void main()
{
    vec4 baseColor = bUseTexture
        ? texture(objectTexture, textureUV * UVscale) : objectColor;
    if (!bUseLighting)
    {
        fragmentColor = baseColor;
        return;
    }

    vec3 normal = normalize(surfaceNormal);
    vec3 viewDirection = normalize(viewPosition - fragmentPosition);
    // Ambient illumination keeps surfaces visible away from direct light.
    vec3 result = baseColor.rgb * material.ambientColor * material.ambientStrength;
    for (int i = 0; i < 2; ++i)
    {
        vec3 offset = lights[i].position - fragmentPosition;
        float distanceToLight = length(offset);
        vec3 lightDirection = offset / max(distanceToLight, 0.0001);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float specular = 0.0;
        // Avoid highlights on the side facing away from the light.
        if (diffuse > 0.0)
        {
            vec3 reflection = reflect(-lightDirection, normal);
            specular = pow(max(dot(viewDirection, reflection), 0.0),
                max(material.shininess, 1.0));
        }
        float attenuation = 1.0 / (1.0 + 0.014 * distanceToLight
            + 0.0007 * distanceToLight * distanceToLight);
        vec3 reflectedLight = baseColor.rgb * material.diffuseColor * diffuse
            + material.specularColor * specular;
        result += reflectedLight * lights[i].color * lights[i].strength * attenuation;
    }
    fragmentColor = vec4(result, baseColor.a);
}
