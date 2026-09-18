// Unlit coloured lines for the AABB debug overlay. Deliberately minimal -- no
// lighting, no textures -- and drawn with GL_LINES, so its vertex layout is
// position + colour rather than the full Vertex struct used by meshes.

#type vertex
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}

#type fragment
#version 330 core

in vec3 vColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
