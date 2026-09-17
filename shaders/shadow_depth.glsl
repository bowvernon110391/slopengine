// Depth-only shader used by ShadowPass to fill a light's shadow map.
//
//   #type vertex   -> transforms geometry into light clip space
//   #type fragment -> empty (depth is the only output)

#type vertex
#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}

#type fragment
#version 330 core

void main()
{
    // Depth is written automatically; no colour output.
}
