// Reusable lighting function, included from a fragment stage with:
//
//     #include "common/lighting.glsl"
//
// Expects the following to already be declared by the including stage:
//   varyings:   vNormal, vColor, vWorldPos
//   uniforms:   uLightDir, uLightColor, uViewPos
//   macros:     SPECULAR_POWER, SPECULAR_STRENGTH (defined in the parent file)

vec3 computeLighting(vec3 normal, vec3 color, vec3 worldPos)
{
    vec3 N = normalize(normal);
    vec3 L = normalize(-uLightDir);         // fragment -> light
    vec3 V = normalize(uViewPos - worldPos);

    float diff = max(dot(N, L), 0.0);

    vec3 ambient = 0.22 * color;
    vec3 diffuse = diff * color * uLightColor;

    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), SPECULAR_POWER) * SPECULAR_STRENGTH;

    return ambient + diffuse + spec;
}
