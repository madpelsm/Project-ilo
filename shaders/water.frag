#version 330 core
// Reflective water: a fresnel mix of a cheap analytic sky reflection (with sun & moon
// glints) over a depth-tinted body colour, rippled by scrolling waves, with a soft
// foam line at the shore. Reads the G-buffer position to know how deep the water is.
in vec3 vWorldPos;
out vec3 FragColor;

uniform sampler2D gPosition; // terrain behind the water (origin-relative; .y is world Y since origin.y=0)
uniform vec2 uScreen;
uniform vec3 eyePos; // true world eye
uniform float uTime;
uniform vec3 uSkyTop, uSkyHorizon, uHorizonGlow;
uniform vec3 uSunDir, uSunColor, uMoonDir, uMoonColor;
uniform vec3 uWaterColor;
uniform float uStarFade;
uniform vec4 uDimple[12]; // fish rises: xy = centre XZ (world), z = spawn time, w unused
uniform int uDimpleCount;
uniform vec3 uAuroraColor;     // woven-constellation hue the Mere catches at night
uniform float uAuroraColorMix; // 0 = default green aurora .. ~0.7 = full woven colour

// Shared aerial fog + the pooling ground-mist so the lake melts into the same air as
// the land. KEEP applyMist IN SYNC with secondPassFrag.frag.
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform sampler2D uMistNoise;
uniform vec3 uMistColor;
uniform float uMistDensity;
uniform float uMistBaseY;
uniform float uMistHeightFalloff;
uniform vec2 uMistOriginXZ; // 0 here (water.vert is true world)
uniform vec3 uSunlight;

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

vec3 reflSky(vec3 r) {
    vec3 c = mix(uSkyHorizon, uSkyTop, pow(max(r.y, 0.0), 0.5));
    c += uSunColor * pow(max(dot(r, normalize(uSunDir)), 0.0), 200.0) * 3.0;   // sun glint
    c += uMoonColor * pow(max(dot(r, normalize(uMoonDir)), 0.0), 400.0) * 5.0 * uStarFade; // moon glint
    c += uMoonColor * pow(max(dot(r, normalize(uMoonDir)), 0.0), 14.0) * 0.25 * uStarFade; // moon glitter
    c += uHorizonGlow * exp(-max(r.y, 0.0) * 5.0) * 0.5;
    // The authored aurora's hue pools faintly in the still water at night.
    c += mix(vec3(0.10, 1.0, 0.5), uAuroraColor, uAuroraColorMix) * smoothstep(0.0, 0.5, r.y) * 0.15 * uStarFade;
    return c;
}

void main() {
    vec2 uv = gl_FragCoord.xy / uScreen;
    float terrainY = texture(gPosition, uv).y;  // world Y of the lake bed behind this pixel
    float depth = max(0.0, -terrainY);           // water surface is at y = 0

    vec3 V = normalize(eyePos - vWorldPos);
    // Ripples: perturb the surface normal with a couple of scrolling wave sets.
    vec2 w = vWorldPos.xz;
    float a = sin(w.x * 0.6 + uTime * 0.8) + sin(w.y * 0.5 - uTime * 0.6);
    float b = sin(w.x * 0.13 - uTime * 0.3) + sin(w.y * 0.17 + uTime * 0.45);
    vec3 nrm = vec3(a * 0.02 + b * 0.04, 1.0, a * 0.02 - b * 0.04);

    // Fish rises: a ring of ripples expands and fades from each dimple, tilting the
    // surface radially so the reflection bends and a faint bright crest catches the light.
    float dimpleHi = 0.0;
    for (int i = 0; i < uDimpleCount; i++) {
        vec2 d = w - uDimple[i].xy;
        float dist = length(d);
        float age = uTime - uDimple[i].z;
        float radius = age * 1.1;                       // crest expands ~1.1 m/s
        float life = clamp(1.0 - age / 2.8, 0.0, 1.0);  // fades over ~2.8 s
        float crest = exp(-pow((dist - radius) * 1.8, 2.0));        // leading ring
        float inner = exp(-pow((dist - radius * 0.5) * 2.4, 2.0)) * 0.5; // trailing ripple
        float wsum = (crest + inner) * life;
        vec2 dir = d / max(dist, 1e-3);
        nrm.xz += dir * wsum * 0.55; // tilt the surface radially so the reflection bends
        dimpleHi += crest * life;
    }

    vec3 N = normalize(nrm);
    vec3 R = reflect(-V, N);
    float fres = 0.02 + 0.98 * pow(1.0 - max(dot(V, N), 0.0), 5.0);
    vec3 deep = uWaterColor * (0.5 + 0.5 * exp(-depth * 0.4)); // a touch lighter where shallow
    vec3 col = mix(deep, reflSky(R), fres);

    float foam = smoothstep(0.7, 0.0, depth); // bright line right at the shoreline
    col = mix(col, vec3(0.65, 0.82, 0.88), foam * 0.5);
    col += (uHorizonGlow + uSkyHorizon) * dimpleHi * 0.4; // crest of each fish ring catches the light

    // Melt the far lake into the same aerial fog as the land, then lay the mist sheet on
    // top so the Mere reads as part of the misted basin, not a hard mirror to the horizon.
    float dist = length(vWorldPos - eyePos);
    float distFog = exp(-pow(dist * uFogDensity, 2.0));
    col = mix(uFogColor, col, clamp(distFog, 0.0, 1.0));
    col = applyMist(col, vWorldPos, eyePos);

    FragColor = col;
}
