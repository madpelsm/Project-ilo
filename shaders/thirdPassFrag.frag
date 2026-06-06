#version 330 core
// Final composite: HDR scene + bloom -> ACES tonemap -> fuel vignette -> grade
// -> dither -> single gamma. This is the ONLY place gamma is applied.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uGodray;
uniform float uExposure;       // ~1.0
uniform float uBloomIntensity; // ~0.6
uniform float uVignetteMax;    // 0..1, strength of the darkening
uniform float uFuel;           // 0..1, lantern warmth -> vignette radius + grade
uniform float uFlash;          // additive white flash on firefly collection
uniform float uTime;           // for animated film grain

vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash21(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

void main() {
    // Subtle chromatic aberration: split the channels radially toward the edges.
    vec2 q0 = TexCoords - 0.5;
    vec2 ca = q0 * 0.0022 * dot(q0, q0) * 4.0;
    vec3 scene;
    scene.r = texture(uScene, TexCoords + ca).r;
    scene.g = texture(uScene, TexCoords).g;
    scene.b = texture(uScene, TexCoords - ca).b;
    vec3 hdr = scene + texture(uBloom, TexCoords).rgb * uBloomIntensity
               + texture(uGodray, TexCoords).rgb; // crepuscular rays
    vec3 mapped = ACESFilm(hdr * uExposure); // exposure before, clamp inside

    // Fuel-driven vignette: low fuel -> small bright island around the centre.
    vec2 q = TexCoords - 0.5;
    float dist = length(q);
    float radius = mix(0.20, 0.75, uFuel);
    float softness = 0.45;
    float vig = smoothstep(radius, radius - softness, dist); // 1 centre -> 0 corners
    vig = mix(1.0 - uVignetteMax, 1.0, vig);                 // floor so corners never pure black
    mapped *= vig;

    // Subtle cool-when-cold / warm-when-cozy grade.
    vec3 cool = vec3(0.82, 0.90, 1.08);
    vec3 warm = vec3(1.10, 1.00, 0.84);
    mapped *= mix(cool, warm, uFuel);

    mapped += (hash21(gl_FragCoord.xy) - 0.5) / 255.0; // dither to kill banding
    vec3 outc = pow(mapped, vec3(1.0 / 2.2));           // single gamma, last

    // Film grain (animated, luminance-scaled so highlights stay clean).
    float g = hash21(gl_FragCoord.xy + fract(uTime) * 311.7) - 0.5;
    float lum = dot(outc, vec3(0.299, 0.587, 0.114));
    outc += g * 0.035 * (0.4 + 0.6 * (1.0 - lum));

    outc += uFlash; // additive flash in display space
    FragColor = vec4(outc, 1.0);
}
