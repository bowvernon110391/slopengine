// Visualises a shadow map's depth texture as a grayscale image.
//
// Shadow maps are depth-only, so their raw contents occupy a narrow band of the
// 0..1 range and look almost flat. The uMin/uMax window lets the UI zoom into
// the band the geometry actually occupies, which is what makes shadow acne and
// bias problems visible.

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

uniform sampler2D uDepth;
uniform float     uMin;
uniform float     uMax;
uniform int       uInvert;

out vec4 FragColor;

void main()
{
    float d = texture(uDepth, vUV).r;

    // Contrast window.
    float t = (uMax > uMin) ? (d - uMin) / (uMax - uMin) : d;
    t = clamp(t, 0.0, 1.0);

    if (uInvert != 0) t = 1.0 - t;

    FragColor = vec4(vec3(t), 1.0);
}
