// Forward shading: one directional light (optionally shadow-mapped) plus an
// array of unshadowed point lights. Both stages live in this single file.

#type vertex
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // Meshes only ever get rigid / uniformly-scaled transforms.
    vNormal = mat3(uModel) * aNormal;
    vColor  = aColor;

    gl_Position = uProjection * uView * worldPos;
}

#type fragment
#version 330 core

#define MAX_POINT_LIGHTS 8

in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;

uniform vec3  uCameraPos;
uniform vec3  uAlbedo;
uniform float uSpecPower;
uniform float uSpecStrength;

// Directional light.
uniform int   uDirLightEnabled;
uniform vec3  uDirLightDir;     // travel direction
uniform vec3  uDirLightColor;   // colour * intensity

// Shadow map (first shadow-casting light).
uniform int       uShadowEnabled;
uniform mat4      uLightSpaceMatrix;
uniform sampler2D uShadowMap;

// Point lights (unshadowed).
uniform int   uPointLightCount;
uniform vec3  uPointPos[MAX_POINT_LIGHTS];
uniform vec3  uPointColor[MAX_POINT_LIGHTS];
uniform float uPointIntensity[MAX_POINT_LIGHTS];
uniform float uPointRange[MAX_POINT_LIGHTS];

#include "common/lighting.glsl"

out vec4 FragColor;

void main()
{
    vec3 albedo = vColor * uAlbedo;
    vec3 lit    = computeLighting(vNormal, albedo, vWorldPos, uCameraPos);
    FragColor   = vec4(lit, 1.0);
}
