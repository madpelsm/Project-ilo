#version 330 core
// Deferred lighting pass. Reads the G-buffer, accumulates all point lights from
// the std140 LightBlock UBO with Blinn-Phong + Karis windowed attenuation, draws
// the night sky where no geometry was rasterized, and applies height/distance fog.
in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D gPosition; // rgb world pos, a emissive strength
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMtlProps;

uniform vec3 eyePos;

struct OmniLight {
    vec4 posRadius;      // xyz pos, w radius
    vec4 colorIntensity; // rgb colour, w intensity
};
layout(std140) uniform LightBlock {
    OmniLight lights[128];
    int uLightCount;
};

uniform vec3 uAmbient;       // hemisphere ambient from ABOVE (cool sky light)
uniform vec3 uAmbientGround; // hemisphere ambient from BELOW (warm ground bounce)

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseY;

// Volumetric ground-mist (pools in the hollows; drifting world-locked fbm).
uniform float uTime;
uniform sampler2D uMistNoise;
uniform vec3 uMistColor;
uniform float uMistDensity;       // distance falloff rate (0 = off)
uniform float uMistBaseY;
uniform float uMistHeightFalloff; // mist thins with height above baseY
uniform vec2 uMistOriginXZ;       // + P.xz -> true world (noise stays world-locked)

uniform sampler2D uSkyTex;  // half-res procedural sky (background + atmosphere)
uniform sampler2D uAO;      // screen-space ambient occlusion (1 open .. 0 occluded)
uniform vec3 uSunDir;       // toward the sun
uniform vec3 uSunlight;     // directional radiance (colour * intensity), ~0 at night
uniform vec3 uRimColor;     // sky-tinted rim light on silhouette edges

// Sun shadow map (directional). highp sampler2DShadow declared inline: the ES loader
// only injects precision for sampler2D, and the qualifier is accepted-and-ignored on
// desktop GLSL 330 — so this one declaration compiles on both targets.
uniform highp sampler2DShadow uShadowMap;
uniform mat4 uLightVP;          // origin-relative world -> sun light clip (near cascade)
uniform float uShadowTexel;     // 1/res (PCF tap step in shadow UV)
uniform float uShadowTexelWorld;// world metres per texel (normal-offset scale)
uniform float uShadowBias;      // constant NDC compare floor
uniform float uShadowStrength;  // 0..1 sun-elevation/night fade (0 disables)
uniform int uShadowDebug;       // 1 = output the shadow factor as grayscale
// Far cascade — a 4x-wider map so distant casters cast too.
uniform highp sampler2DShadow uShadowMapFar;
uniform mat4 uLightVPFar;
uniform float uShadowTexelFar;
uniform float uShadowTexelWorldFar;
uniform float uShadowBiasFar;
uniform int uNoFarShadow;
// Contact-hardening (PCSS-lite): the same near depth read raw (no compare) for the
// blocker search, on unit 8 through a no-compare sampler object.
uniform highp sampler2D uShadowDepthRaw;
uniform float uShadowSunSize;     // penumbra growth with blocker distance (0 = plain PCF)
uniform float uShadowMaxPenumbra; // UV cap on the soft radius
uniform int uShadowTaps;          // Vogel-disk taps for the soft filter

vec3 applyFog(vec3 col, vec3 P) {
    float dist = length(P - eyePos);
    float distFog = exp(-pow(dist * uFogDensity, 2.0)); // 1 clear .. 0 fogged
    float h = clamp(exp(-(P.y - uFogBaseY) * uFogHeightFalloff), 0.0, 1.0);
    float fog = clamp(mix(1.0, distFog, h), 0.0, 1.0);
    return mix(uFogColor, col, fog); // fog==1 -> unfogged
}

// Ground-pooling mist: thick low and far (the ray crosses more of the thin band where
// the land is low), clear on ridges; a slow world-locked drifting fbm breaks it up and
// a forward-scatter glow toward the sun gives the dawn "silver air".
vec3 applyMist(vec3 col, vec3 P, vec3 eye) {
    if (uMistDensity <= 0.0)
        return col;
    float heightFactor = exp(-uMistHeightFalloff * max(P.y - uMistBaseY, 0.0));
    float dist = length(P - eye);
    float distFactor = 1.0 - exp(-dist * uMistDensity);
    vec2 np = (P.xz + uMistOriginXZ) * 0.01;
    float n = texture(uMistNoise, np + uTime * 0.004).r * 0.65 + texture(uMistNoise, np * 2.7 - uTime * 0.006).r * 0.35;
    float m = clamp(heightFactor * distFactor * (0.4 + 1.1 * n), 0.0, 0.88);
    vec3 rd = normalize(P - eye);
    float scat = pow(max(dot(rd, normalize(uSunDir)), 0.0), 8.0);
    vec3 mc = uMistColor + uSunlight * scat * 0.5;
    return mix(col, mc, m);
}

// 3x3 hardware-PCF (each tap a free 2x2 bilinear compare -> ~6x6 soft footprint).
float pcf(highp sampler2DShadow sm, vec2 uv, float ref, float texel) {
    float s = 0.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            s += texture(sm, vec3(uv + vec2(float(i), float(j)) * texel, ref));
    return s / 9.0;
}
// Interleaved-gradient noise + Vogel disk for a per-pixel-rotated soft kernel.
float ign(vec2 p) { return fract(52.9829189 * fract(0.06711056 * p.x + 0.00583715 * p.y)); }
vec2 vogel(int i, int n, float rot) {
    float r = sqrt((float(i) + 0.5) / float(n));
    float theta = float(i) * 2.39996323 + rot * 6.2831853;
    return r * vec2(cos(theta), sin(theta));
}
// Contact-hardening near-cascade shadow: estimate the average blocker depth, grow the
// penumbra with the receiver->blocker gap (sharp at contact, soft at the shadow tip),
// then a rotated Vogel-disk hardware-PCF at that radius.
float pcssNear(vec3 q, float rot) {
    float searchR = uShadowTexel * 5.0;
    float bSum = 0.0, bCnt = 0.0;
    for (int i = 0; i < 16; i++) {
        float d = texture(uShadowDepthRaw, q.xy + vogel(i, 16, rot) * searchR).r;
        if (d < q.z - uShadowBias) {
            bSum += d;
            bCnt += 1.0;
        }
    }
    if (bCnt < 0.5)
        return 1.0; // no blockers -> fully lit
    float pen = clamp((q.z - bSum / bCnt) * uShadowSunSize, uShadowTexel, uShadowMaxPenumbra);
    float s = 0.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uShadowTaps)
            break;
        s += texture(uShadowMap, vec3(q.xy + vogel(i, uShadowTaps, rot) * pen, q.z - uShadowBias));
    }
    return s / float(uShadowTaps);
}
float sampleFar(vec3 P, vec3 N, float no) {
    vec3 Po = P + N * uShadowTexelWorldFar * no;
    vec4 lc = uLightVPFar * vec4(Po, 1.0);
    vec3 q = lc.xyz / lc.w * 0.5 + 0.5;
    if (q.z > 1.0 || q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0)
        return 1.0;
    return pcf(uShadowMapFar, q.xy, q.z - uShadowBiasFar, uShadowTexelFar);
}
// Soft cascaded sun shadow: the crisp 128m near map where it covers, the wide far map
// beyond, with a blend ring over the near edge to hide the seam. Receiver nudged along
// its normal to kill acne. 1 lit .. 0 shadowed; out-of-both = lit; off at night.
float sunShadow(vec3 P, vec3 N, float ndl) {
    if (uShadowStrength <= 0.0)
        return 1.0;
    float no = 1.0 + 2.0 * (1.0 - clamp(ndl, 0.0, 1.0)); // grazing-angle normal-offset
    vec3 Po = P + N * uShadowTexelWorld * no;
    vec4 lc = uLightVP * vec4(Po, 1.0);
    vec3 q = lc.xyz / lc.w * 0.5 + 0.5;
    float inset = uShadowTexel * 1.5;
    bool inNear = q.z <= 1.0 && q.x >= inset && q.x <= 1.0 - inset && q.y >= inset && q.y <= 1.0 - inset;
    if (inNear) {
        // Contact-hardening soft shadow in the near cascade (plain 3x3 if disabled).
        float nearSh = uShadowSunSize > 0.0 ? pcssNear(q, ign(gl_FragCoord.xy))
                                            : pcf(uShadowMap, q.xy, q.z - uShadowBias, uShadowTexel);
        if (uNoFarShadow == 1)
            return nearSh;
        float edge = smoothstep(0.85, 1.0, max(abs(q.x * 2.0 - 1.0), abs(q.y * 2.0 - 1.0)));
        if (edge > 0.0)
            return mix(nearSh, sampleFar(P, N, no), edge);
        return nearSh;
    }
    if (uNoFarShadow == 1)
        return 1.0;
    return sampleFar(P, N, no);
}

void main() {
    vec3 N = texture(gNormal, TexCoords).rgb;
    if (dot(N, N) < 0.25) { // background: no geometry here -> the procedural sky
        FragColor = texture(uSkyTex, TexCoords).rgb;
        return;
    }
    N = normalize(N);

    vec4 Pe = texture(gPosition, TexCoords);
    vec3 P = Pe.rgb;
    float emissive = Pe.a;
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    vec4 m = texture(gMtlProps, TexCoords);
    float shininess = m.x * 256.0;
    float specStrength = m.y;
    float ambient = m.z;

    vec3 V = normalize(eyePos - P);
    // Ambient occlusion only darkens the indirect/fill terms (ambient + sky rim), never
    // the direct sun or point lights — so creases and contact points read as shadowed
    // without dimming surfaces a real light actually reaches.
    float ao = texture(uAO, TexCoords).r;
    // Hemisphere sky-light: upward faces catch the cool sky, downward faces a warm
    // ground bounce — gives shadowed/unlit surfaces realistic directional fill.
    vec3 hemi = mix(uAmbientGround, uAmbient, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 lit = (ambient + hemi) * albedo * ao;

    // Directional sunlight (no shadows): soft-wrap so shadowed sides never go pure
    // black. ~0 at night, so the cosy point-light glow still owns the dark.
    float sunSh = 1.0;
    {
        vec3 Ls = normalize(uSunDir);
        float ndl = dot(N, Ls);
        float wrap = ndl * 0.85 + 0.15;
        vec3 Hs = normalize(Ls + V);
        float sd = smoothstep(0.0, 0.05, ndl) * pow(max(dot(N, Hs), 0.0), shininess) * specStrength;
        sunSh = mix(1.0, sunShadow(P, N, ndl), uShadowStrength); // cast sun shadow
        lit += uSunlight * (max(wrap, 0.0) * albedo + sd) * sunSh;
    }

    for (int i = 0; i < uLightCount; ++i) {
        vec3 toL = lights[i].posRadius.xyz - P;
        float r = lights[i].posRadius.w;
        float d2 = dot(toL, toL);
        if (d2 > r * r)
            continue; // hard radius cull
        float d = sqrt(max(d2, 1e-8));
        vec3 L = toL / d;
        vec3 H = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        // Gate specular by the diffuse term so highlights cannot leak past the
        // terminator onto surfaces the light does not actually reach.
        float spec = smoothstep(0.0, 0.05, dot(N, L)) * pow(max(dot(N, H), 0.0), shininess) * specStrength;
        float rr = d / r;
        float window = clamp(1.0 - rr * rr * rr * rr, 0.0, 1.0); // Karis windowed inverse-square
        float att = (window * window) / (d2 + 1.0) * lights[i].colorIntensity.w;
        lit += att * lights[i].colorIntensity.rgb * (diff * albedo + spec);
    }

    // Rim / sky light: a fresnel sheen on silhouette edges, tinted by the sky and lifted
    // by how skyward the surface faces — gives the low-poly forms a soft glowing edge.
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * (0.4 + 0.6 * max(N.y, 0.0));
    lit += rim * uRimColor * albedo * ao;

    lit = applyFog(lit, P);
    lit = applyMist(lit, P, eyePos); // pooling ground-mist on top of the aerial fog
    lit += albedo * emissive; // emissive after fog so glowing sources punch through
    if (uShadowDebug == 1) { // ILO_SHADOWDEBUG: prove registration/PCF independent of tonemap
        FragColor = vec3(sunSh);
        return;
    }
    FragColor = lit;
}
