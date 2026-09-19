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
//               uShadowBias, uShadowSlopeBiasScale, uShadowMaxBias,
//               uShadowPcfRadius,
//               uPointLightCount, uPointPos[], uPointColor[],
//               uPointIntensity[], uPointRange[]
//   macro:      MAX_POINT_LIGHTS

// Depth bias for the shadow comparison.
//
// The depth error of a shadow lookup grows with the surface's slope relative to
// the light, proportional to tan(theta). A constant bias therefore either leaks
// light on steep surfaces or detaches shadows on flat ones. uShadowSlopeBiasScale
// widens the bias as the surface turns away; uShadowMaxBias caps it so a nearly
// edge-on surface cannot bias itself out of range and lose its shadow entirely.
//
// These trade directly against each other, which is why they are tunable at
// runtime rather than fixed: a smaller bias keeps a shadow attached to the object
// casting it but lets acne through, and a larger bias does the reverse. On the
// flat ground the value used is uShadowBias * (1 + uShadowSlopeBiasScale * tan),
// clamped to uShadowMaxBias.
//
// Declared by the including stage, like the other uniforms in the list above --
// declaring them here as well would be a duplicate declaration in any stage that
// includes this file, which fails to compile.

// tan(acos(ndl)) computed without the trig: sqrt(1 - c^2) / c.
float slopeScaledBias(float ndl)
{
    float c = clamp(ndl, 1e-4, 1.0);
    float tanTheta = sqrt(1.0 - c * c) / c;
    return min(uShadowBias * (1.0 + uShadowSlopeBiasScale * tanTheta),
               uShadowMaxBias);
}

// 16-point Poisson disc used to filter the shadow lookup.
//
// Maximin-optimised on a unit disc: the closest pair of taps is 0.449 apart,
// against ~0.476 for ideal hex packing of 16 points, so the disc is within a few
// percent of the best spacing achievable at this tap count. The centroid sits on
// the origin and every radius is <= 1, so scaling by a radius R gives a footprint
// that reaches R texels in every direction.
//
// Regenerate with the generator script rather than editing by hand -- an
// unverified table tends to leave a hole, which shows up as a bright ring.
const vec2 kPoissonDisc16[16] = vec2[16](
    vec2(-0.79546, -0.32911),
    vec2(-0.56759, -0.77051),
    vec2( 0.42642,  0.27320),
    vec2(-0.10305, -0.72944),
    vec2(-0.01728,  0.40411),
    vec2(-0.41297,  0.16755),
    vec2(-0.13625,  0.93186),
    vec2( 0.87282,  0.45587),
    vec2(-0.31162, -0.26983),
    vec2( 0.17817, -0.21919),
    vec2( 0.69216, -0.15067),
    vec2( 0.50373,  0.82407),
    vec2(-0.53221,  0.68216),
    vec2( 0.78067, -0.62495),
    vec2(-0.89206,  0.25323),
    vec2( 0.31453, -0.89836));

// Interleaved gradient noise (Jimenez), returning [0, 1). Cheaper than a bit hash
// and better distributed than a single fract() of the pixel coordinates.
float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

// Poisson-disc shadow lookup against the directional light's depth map.
//
// 16 taps on a disc, scaled by uShadowPcfRadius and rotated per pixel. A wider
// radius softens the penumbra at a fixed 16 fetches; the rotation is what keeps
// that tap count from reading as structured banding, because it decorrelates the
// pattern from the shadow map's texel grid instead of leaving it aligned to it.
float sampleShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec4 lightSpace = uLightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 1.0;   // beyond the light far plane

    float bias = slopeScaledBias(dot(N, L));
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    vec2 stepSize = texel * uShadowPcfRadius;

    // A fixed disc orientation would show its own structure at 16 taps; a
    // per-pixel angle turns that into fine noise instead.
    float angle = interleavedGradientNoise(gl_FragCoord.xy) * 6.28318530718;

    // Everything above is per-pixel, so the rotation and the biased reference depth
    // are computed once here rather than once per tap. The loop is fetch-bound, so
    // this is for clarity, not speed.
    float sinA = sin(angle);
    float cosA = cos(angle);
    float refZ = proj.z - bias;

    float visible = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 d = kPoissonDisc16[i];   // plain local: a const initialised from a
                                      // non-constant is a GLSL compile error
        vec2 offset = vec2(cosA * d.x - sinA * d.y,
                           sinA * d.x + cosA * d.y) * stepSize;
        float depth = texture(uShadowMap, proj.xy + offset).r;
        visible += (refZ > depth) ? 0.0 : 1.0;
    }
    return visible / 16.0;
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
