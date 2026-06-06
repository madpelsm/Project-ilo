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
uniform mat4 invViewProj;

struct OmniLight {
    vec4 posRadius;      // xyz pos, w radius
    vec4 colorIntensity; // rgb colour, w intensity
};
layout(std140) uniform LightBlock {
    OmniLight lights[128];
    int uLightCount;
};

uniform vec3 uAmbient; // global cool moonlight fill so the forest isn't a black void

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseY;

uniform vec3 uSkyTop;
uniform vec3 uSkyHorizon;
uniform vec3 uMoonDir;
uniform vec3 uMoonColor;
uniform float uMoonSize;

vec3 applyFog(vec3 col, vec3 P) {
    float dist = length(P - eyePos);
    float distFog = exp(-pow(dist * uFogDensity, 2.0)); // 1 clear .. 0 fogged
    float h = clamp(exp(-(P.y - uFogBaseY) * uFogHeightFalloff), 0.0, 1.0);
    float fog = clamp(mix(1.0, distFog, h), 0.0, 1.0);
    return mix(uFogColor, col, fog); // fog==1 -> unfogged
}

vec3 skyColor(vec3 ray) {
    vec3 sky = mix(uSkyHorizon, uSkyTop, clamp(ray.y, 0.0, 1.0));
    // soft moon disc
    float m = smoothstep(uMoonSize, uMoonSize * 0.7, distance(ray, normalize(uMoonDir)));
    sky += uMoonColor * m;
    // faint glow halo around the moon
    sky += uMoonColor * 0.15 * pow(max(dot(ray, normalize(uMoonDir)), 0.0), 8.0);
    return sky;
}

void main() {
    vec3 N = texture(gNormal, TexCoords).rgb;
    if (dot(N, N) < 0.25) { // background: no geometry here -> draw sky
        vec3 ndc = vec3(TexCoords * 2.0 - 1.0, 1.0);
        vec4 wp = invViewProj * vec4(ndc, 1.0);
        vec3 ray = normalize(wp.xyz / wp.w - eyePos);
        FragColor = skyColor(ray);
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
        float spec = pow(max(dot(N, H), 0.0), shininess) * specStrength;
        float rr = d / r;
        float window = clamp(1.0 - rr * rr * rr * rr, 0.0, 1.0); // Karis windowed inverse-square
        float att = (window * window) / (d2 + 1.0) * lights[i].colorIntensity.w;
        lit += att * lights[i].colorIntensity.rgb * (diff * albedo + spec);
    }

    lit = applyFog(lit, P);
    lit += albedo * emissive; // emissive after fog so glowing sources punch through
    FragColor = lit;
}
