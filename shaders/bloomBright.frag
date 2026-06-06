#version 330 core
// Bright-pass: keep only the part of each pixel above a soft luminance threshold.
in vec2 TexCoords;
out vec3 FragColor;
uniform sampler2D uScene;
uniform float uThreshold; // ~1.0
uniform float uSoftKnee;  // ~0.5

void main() {
    vec3 c = texture(uScene, TexCoords).rgb;
    float br = max(c.r, max(c.g, c.b));
    float knee = uThreshold * uSoftKnee + 1e-4;
    float rq = clamp(br - (uThreshold - knee), 0.0, 2.0 * knee);
    rq = (0.25 / knee) * rq * rq;
    float w = max(rq, br - uThreshold) / max(br, 1e-4); // guarded -> no NaN on black
    FragColor = c * w;
}
