#version 330 core
// Cheap forward shading for the reflected world (no point lights / SSAO / rim — a rippled
// half-res mirror does not need them): hemisphere ambient + a single-tap sun shadow +
// emissive, then the SAME aerial fog + pooling mist as the main pass so the reflected far
// shore melts consistently. Alpha = 1 marks "geometry here" so the water shader keeps the
// analytic sky reflection where there is none. Underwater geometry is discarded.
// KEEP applyFog/applyMist IN SYNC with secondPassFrag.frag.
in vec3 vPosRel;
in vec3 vNormal;
in vec3 vAlbedo;
in vec3 vMtlProps;
in float vEmissive;
in float vWorldY;
out vec4 FragColor;

uniform vec3 eyePos; // origin-relative
uniform vec3 uAmbient, uAmbientGround;
uniform vec3 uSunDir, uSunlight;
uniform mat4 uLightVP;
uniform highp sampler2DShadow uShadowMap;
uniform float uShadowBias, uShadowStrength;
uniform vec3 uFogColor;
uniform float uFogDensity, uFogHeightFalloff, uFogBaseY;
uniform sampler2D uMistNoise;
uniform vec3 uMistColor;
uniform float uMistDensity, uMistBaseY, uMistHeightFalloff, uTime;
uniform vec2 uMistOriginXZ;

float shadow1tap(vec3 P, vec3 N) {
    if (uShadowStrength <= 0.0)
        return 1.0;
    vec3 Po = P + N * 0.2;
    vec4 lc = uLightVP * vec4(Po, 1.0);
    vec3 q = lc.xyz / lc.w * 0.5 + 0.5;
    if (q.z > 1.0 || q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0)
        return 1.0;
    return mix(1.0, texture(uShadowMap, vec3(q.xy, q.z - uShadowBias)), uShadowStrength);
}
vec3 applyFog(vec3 c, vec3 P) {
    float d = length(P - eyePos);
    float df = exp(-pow(d * uFogDensity, 2.0));
    float h = clamp(exp(-(P.y - uFogBaseY) * uFogHeightFalloff), 0.0, 1.0);
    return mix(uFogColor, c, clamp(mix(1.0, df, h), 0.0, 1.0));
}
vec3 applyMist(vec3 col, vec3 P) {
    if (uMistDensity <= 0.0)
        return col;
    float hf = exp(-uMistHeightFalloff * max(P.y - uMistBaseY, 0.0));
    float dist = length(P - eyePos);
    float dfac = 1.0 - exp(-dist * uMistDensity);
    vec2 np = (P.xz + uMistOriginXZ) * 0.01;
    float n = texture(uMistNoise, np + uTime * 0.004).r * 0.65 + texture(uMistNoise, np * 2.7 - uTime * 0.006).r * 0.35;
    float m = clamp(hf * dfac * (0.4 + 1.1 * n), 0.0, 0.88);
    return mix(col, uMistColor, m);
}

void main() {
    if (vWorldY < -0.02)
        discard; // do not reflect the lake bed / submerged geometry
    vec3 N = normalize(vNormal);
    vec3 hemi = mix(uAmbientGround, uAmbient, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 lit = (vMtlProps.z + hemi) * vAlbedo;
    vec3 Ls = normalize(uSunDir);
    float wrap = dot(N, Ls) * 0.85 + 0.15;
    lit += uSunlight * max(wrap, 0.0) * vAlbedo * shadow1tap(vPosRel, N);
    lit = applyFog(lit, vPosRel);
    lit = applyMist(lit, vPosRel);
    lit += vAlbedo * vEmissive;
    FragColor = vec4(lit, 1.0); // alpha 1 -> water uses this instead of analytic sky
}
