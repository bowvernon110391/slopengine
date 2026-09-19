// Reusable multi-light forward lighting, included from a fragment stage with:
//
//     #include "common/lighting.glsl"
//
// The including stage must declare the following before the #include:
//   varyings:   vNormal, vColor, vWorldPos
//   uniforms:   uCameraPos, uAlbedo, uSpecPower, uSpecStrength, uAmbient,
//               uDirLightEnabled, uDirLightDir, uDirLightColor,
//               uShadowEnabled, uLightSpaceMatrix, uShadowMap,
//               uShadowDistance, uShadowFade,
//               uPointLightCount, uPointPos[], uPointColor[],
//               uPointIntensity[], uPointRange[]
//   macro:      MAX_POINT_LIGHTS

// Depth bias for the shadow comparison.
//
// The depth error of a shadow lookup grows with the surface's slope relative to
// the light, proportional to tan(theta). A constant bias therefore either leaks
// light on steep surfaces or detaches shadows on flat ones. kSlopeBiasScale
// widens the bias as the surface turns away; kMaxShadowBias caps it so a nearly
// edge-on surface cannot bias itself out of range and lose its shadow entirely.
const float kShadowBias     = 0.0015;
const float kSlopeBiasScale = 0.35;
const float kMaxShadowBias  = 0.02;

// tan(acos(ndl)) computed without the trig: sqrt(1 - c^2) / c.
float slopeScaledBias(float ndl)
{
    float c = clamp(ndl, 1e-4, 1.0);
    float tanTheta = sqrt(1.0 - c * c) / c;
    return min(kShadowBias * (1.0 + kSlopeBiasScale * tanTheta), kMaxShadowBias);
}

// 3x3 PCF shadow lookup against the directional light's depth map.
float sampleShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec4 lightSpace = uLightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 1.0;   // beyond the light far plane

    float bias = slopeScaledBias(dot(N, L));
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));

    float visible = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(uShadowMap, proj.xy + vec2(x, y) * texel).r;
            visible += (proj.z - bias > depth) ? 0.0 : 1.0;
        }
    }
    return visible / 9.0;
}

vec3 computeLighting(vec3 normal, vec3 baseColor, vec3 worldPos, vec3 cameraPos)
{
    vec3 N = normalize(normal);
    vec3 V = normalize(cameraPos - worldPos);

    // Ambient: incoming irradiance scaled by albedo, so a black surface stays
    // black. Added unconditionally -- indirect light is not shadowed.
    vec3 result = uAmbient * baseColor;

    if (uDirLightEnabled != 0) {
        vec3 L = normalize(-uDirLightDir);
        float ndl = max(dot(N, L), 0.0);

        float shadow = 1.0;
        if (uShadowEnabled != 0) {
            shadow = sampleShadow(worldPos, N, L);

            // Fade the shadow out over the tail of the fitted range, so the
            // boundary reads as a soft gradient rather than a hard line where
            // distant geometry stops casting.
            float fadeStart = uShadowDistance * (1.0 - clamp(uShadowFade, 0.0, 1.0));
            float viewDist  = distance(worldPos, cameraPos);
            float fade      = 1.0 - smoothstep(fadeStart, uShadowDistance, viewDist);
            shadow = mix(1.0, shadow, fade);   // 1.0 == fully lit
        }

        vec3 diffuse = ndl * baseColor * uDirLightColor;
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), uSpecPower) * uSpecStrength * ndl;

        result += shadow * (diffuse + spec * uDirLightColor);
    }

    for (int i = 0; i < uPointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 toLight = uPointPos[i] - worldPos;
        float dist = length(toLight);
        vec3 L = (dist > 0.0001) ? toLight / dist : vec3(0.0, 1.0, 0.0);
        float ndl = max(dot(N, L), 0.0);

        float range = max(uPointRange[i], 0.0001);
        float atten = 1.0 / (1.0 + (dist * dist) / (range * range));

        vec3 radiance = uPointColor[i] * uPointIntensity[i] * atten;
        vec3 diffuse = ndl * baseColor * radiance;

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), uSpecPower) * uSpecStrength * ndl;

        result += diffuse + spec * radiance;
    }

    return result;
}
