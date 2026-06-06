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

vec3 reflSky(vec3 r) {
    vec3 c = mix(uSkyHorizon, uSkyTop, pow(max(r.y, 0.0), 0.5));
    c += uSunColor * pow(max(dot(r, normalize(uSunDir)), 0.0), 200.0) * 3.0;   // sun glint
    c += uMoonColor * pow(max(dot(r, normalize(uMoonDir)), 0.0), 400.0) * 5.0 * uStarFade; // moon glint
    c += uMoonColor * pow(max(dot(r, normalize(uMoonDir)), 0.0), 14.0) * 0.25 * uStarFade; // moon glitter
    c += uHorizonGlow * exp(-max(r.y, 0.0) * 5.0) * 0.5;
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
    vec3 N = normalize(vec3(a * 0.02 + b * 0.04, 1.0, a * 0.02 - b * 0.04));

    vec3 R = reflect(-V, N);
    float fres = 0.02 + 0.98 * pow(1.0 - max(dot(V, N), 0.0), 5.0);
    vec3 deep = uWaterColor * (0.5 + 0.5 * exp(-depth * 0.4)); // a touch lighter where shallow
    vec3 col = mix(deep, reflSky(R), fres);

    float foam = smoothstep(0.7, 0.0, depth); // bright line right at the shoreline
    col = mix(col, vec3(0.65, 0.82, 0.88), foam * 0.5);

    FragColor = col;
}
