#version 330 core
// Procedural HDR sky, rendered to a half-res buffer and sampled by the lighting pass
// (and later by water reflections). Gradient + sun glow + sun/moon discs + galaxy band
// + twinkling stars + drifting fbm clouds + night aurora. Driven entirely by the Sky
// time-of-day uniforms. Output is linear HDR (bloom + tonemap happen downstream).
in vec2 TexCoords;
out vec3 FragColor;

uniform mat4 invViewProj;
uniform vec3 eyePos;
uniform float uTime;

uniform vec3 uSkyTop, uSkyHorizon, uHorizonGlow;
uniform vec3 uSunDir, uMoonDir;
uniform vec3 uSunDiscColor, uMoonColor, uSunlight;
uniform float uSunDiscSize, uMoonSize;
uniform float uStarFade, uAuroraStrength, uGalaxyStrength, uCloudCoverage;
uniform vec2 uCloudWind;

float hash21(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float hash31(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + vec2(1, 0)), c = hash21(i + vec2(0, 1)), d = hash21(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        s += a * vnoise(p);
        p *= 2.02;
        a *= 0.5;
    }
    return s;
}

vec3 stars(vec3 ray) {
    if (ray.y < 0.02)
        return vec3(0.0);
    vec3 cell = floor(ray * 130.0);
    float h = hash31(cell);
    float s = smoothstep(0.992, 1.0, h);
    float tw = 0.55 + 0.45 * sin(uTime * 2.5 + h * 50.0);
    return vec3(0.9, 0.95, 1.0) * s * tw * smoothstep(0.0, 0.18, ray.y);
}

// One occasional shooting star, in a slowly-moving lane.
vec3 shootingStar(vec3 ray) {
    if (ray.y < 0.1)
        return vec3(0.0);
    float slot = floor(uTime / 7.0);
    float seed = hash21(vec2(slot, 11.0));
    if (seed < 0.55)
        return vec3(0.0); // most slots: none
    float t = fract(uTime / 7.0);
    vec3 a = normalize(vec3(seed * 2.0 - 1.0, 0.6, -0.5 + seed));
    vec3 b = normalize(a + vec3(0.6, -0.35, 0.2));
    vec3 p = normalize(mix(a, b, t));
    float d = distance(ray, p);
    float head = smoothstep(0.02, 0.0, d);
    float trail = smoothstep(0.06, 0.0, distance(ray, normalize(mix(a, b, max(0.0, t - 0.05))))) * 0.5;
    return vec3(1.0, 0.95, 0.85) * (head + trail) * smoothstep(0.0, 0.1, t) * smoothstep(1.0, 0.85, t);
}

void main() {
    vec3 ndc = vec3(TexCoords * 2.0 - 1.0, 1.0);
    vec4 wp = invViewProj * vec4(ndc, 1.0);
    vec3 ray = normalize(wp.xyz / wp.w - eyePos);

    // Base gradient (saturated horizon band fattened by the pow).
    float h = pow(max(ray.y, 0.0), 0.42);
    vec3 col = mix(uSkyHorizon, uSkyTop, h);

    // Twilight glow hugging the horizon toward the sun's azimuth.
    float horizonBand = exp(-max(ray.y, 0.0) * 6.0);
    vec3 sunAzD = normalize(vec3(uSunDir.x, 0.0, uSunDir.z));
    vec3 rayAzD = normalize(vec3(ray.x, 0.001, ray.z));
    float toward = pow(max(dot(rayAzD, sunAzD), 0.0), 2.0);
    col += uHorizonGlow * horizonBand * toward;

    // Galaxy band (a soft great-circle ribbon of stars).
    if (ray.y > 0.0 && uGalaxyStrength > 0.001) {
        float band = abs(dot(ray, normalize(vec3(0.6, 0.5, 0.35))));
        float g = smoothstep(0.32, 0.0, band) * (0.4 + 0.6 * fbm(ray.xz * 5.0 + ray.y * 4.0));
        col += vec3(0.45, 0.42, 0.7) * g * 0.10 * uGalaxyStrength;
    }

    // Stars + shooting stars.
    col += stars(ray) * uStarFade;
    col += shootingStar(ray) * uStarFade;

    // Sun glow + disc.
    float mu = dot(ray, uSunDir);
    col += uSunDiscColor * 0.5 * pow(max(mu, 0.0), 12.0) * max(uSunDir.y + 0.15, 0.0);
    float sunDisc = smoothstep(1.0 - uSunDiscSize, 1.0 - uSunDiscSize * 0.4, mu);
    col += uSunDiscColor * 6.0 * sunDisc;

    // Moon disc + halo (mostly at night).
    float md = dot(ray, uMoonDir);
    col += uMoonColor * 0.15 * pow(max(md, 0.0), 8.0);
    float moonDisc = smoothstep(1.0 - uMoonSize * 0.18, 1.0 - uMoonSize * 0.07, md);
    col += uMoonColor * 4.0 * moonDisc;

    // Drifting clouds on a high plane (fbm), with a sun-lit silver edge.
    if (ray.y > 0.03) {
        vec2 cp = ray.xz / ray.y * 0.5 + uCloudWind * uTime;
        float c = fbm(cp * 1.4);
        float cover = smoothstep(1.0 - uCloudCoverage, 1.0, c);
        float lit = fbm((cp + normalize(uSunDir.xz + vec2(1e-3)) * 0.06) * 1.4);
        float light = clamp((c - lit) * 3.0 + 0.45, 0.0, 1.0);
        vec3 cloudCol = mix(vec3(0.18, 0.20, 0.26), uSunlight * 0.7 + vec3(0.30), light);
        float edge = smoothstep(0.03, 0.28, ray.y);
        col = mix(col, cloudCol, cover * edge * 0.85);
    }

    // Aurora curtains (night), green->magenta, dipping toward the horizon.
    if (uAuroraStrength > 0.001 && ray.y > 0.04) {
        vec2 ap = ray.xz / max(ray.y, 0.12);
        float a = 0.0;
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            float curtain = fbm(vec2(ap.x * 1.4 + fi * 3.1 + uTime * 0.03, ap.y * 0.55 + uTime * 0.02));
            float band = smoothstep(0.55, 0.92, curtain) * smoothstep(1.6, 0.3, length(ap));
            a += band * (0.6 - fi * 0.15);
        }
        vec3 auroraCol = mix(vec3(0.10, 1.0, 0.5), vec3(0.7, 0.3, 1.0), fbm(ap + uTime * 0.05));
        col += auroraCol * a * uAuroraStrength;
    }

    FragColor = max(col, vec3(0.0));
}
