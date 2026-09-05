// Both the vertex and fragment stages live in this single file, separated by
// "#type" markers. Reusable GLSL can be pulled in with #include (see the
// fragment stage below). Flags may be toggled with #define.
//
//   vertex stage   -> "#type vertex"
//   fragment stage -> "#type fragment"

#type vertex
#version 330 core

// Vertex attributes (see Mesh.cpp for the layout):
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

    // Our meshes only ever get rigid / uniformly-scaled transforms, so the
    // upper-left 3x3 of the model matrix is sufficient to rotate normals.
    vNormal = mat3(uModel) * aNormal;
    vColor  = aColor;

    gl_Position = uProjection * uView * worldPos;
}

#type fragment
#version 330 core

// --- Flags (reportable from C++ via Shader::defines() / hasDefine()) -------
#define SPECULAR_POWER    32.0
#define SPECULAR_STRENGTH 0.35

in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;

uniform vec3 uLightDir;    // direction the light travels (pointing away from the light)
uniform vec3 uLightColor;
uniform vec3 uViewPos;     // camera position, for specular

#include "common/lighting.glsl"

out vec4 FragColor;

void main()
{
    vec3 lit = computeLighting(vNormal, vColor, vWorldPos);
    FragColor = vec4(lit, 1.0);
}
