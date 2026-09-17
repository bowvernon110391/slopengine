// Full-screen blit used by post passes (FinalPostPass today; the SSAO/Bloom
// passes will reuse it once implemented).

#type vertex
#version 330 core

out vec2 vUV;

void main()
{
    // Full-screen triangle generated from gl_VertexID (no vertex buffer).
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 330 core

in vec2 vUV;

uniform sampler2D uSource;

out vec4 FragColor;

void main()
{
    FragColor = vec4(texture(uSource, vUV).rgb, 1.0);
}
