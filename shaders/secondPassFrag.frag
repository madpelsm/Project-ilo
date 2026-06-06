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

uniform vec3 uAmbient; // global ambient fill (sky-driven, follows time of day)

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseY;

uniform sampler2D uSkyTex;  // half-res procedural sky (background + atmosphere)
uniform vec3 uSunDir;       // toward the sun
uniform vec3 uSunlight;     // directional radiance (colour * intensity), ~0 at night

vec3 applyFog(vec3 col, vec3 P) {
    float dist = length(P - eyePos);
    float distFog = exp(-pow(dist * uFogDensity, 2.0)); // 1 clear .. 0 fogged
    float h = clamp(exp(-(P.y - uFogBaseY) * uFogHeightFalloff), 0.0, 1.0);
    float fog = clamp(mix(1.0, distFog, h), 0.0, 1.0);
    return mix(uFogColor, col, fog); // fog==1 -> unfogged
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
    vec3 lit = (ambient + uAmbient) * albedo;

    // Directional sunlight (no shadows): soft-wrap so shadowed sides never go pure
    // black. ~0 at night, so the cosy point-light glow still owns the dark.
    {
        vec3 Ls = normalize(uSunDir);
        float ndl = dot(N, Ls);
        float wrap = ndl * 0.85 + 0.15;
        vec3 Hs = normalize(Ls + V);
        float sd = smoothstep(0.0, 0.05, ndl) * pow(max(dot(N, Hs), 0.0), shininess) * specStrength;
        lit += uSunlight * (max(wrap, 0.0) * albedo + sd);
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

    lit = applyFog(lit, P);
    lit += albedo * emissive; // emissive after fog so glowing sources punch through
    FragColor = lit;
}
