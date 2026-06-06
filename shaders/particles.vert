#version 330 core
// Drifting atmosphere motes (pollen by day, embers by night). Positions are computed
// entirely on the GPU from a static seed + time, wrapped into a box that follows the
// eye, so there is zero per-frame CPU cost. Drawn as additive points into the HDR scene.
layout (location = 0) in vec4 aSeed; // xyz in [0,1], w = phase

uniform mat4 persp;
uniform mat4 view;
uniform vec3 uEye;
uniform float uTime;
uniform float uBoxR;   // half-size of the box around the eye
uniform vec3 uDrift;   // slow drift velocity (wind + rise)
uniform float uSizePx; // base point size in pixels at 1m

out float vFade;

void main() {
    float R = uBoxR;
    vec3 drift = uDrift * uTime;
    // Tile the seed box around the eye so motes are always present nearby.
    vec3 wpos = mod(aSeed.xyz * (2.0 * R) + drift + aSeed.w * 37.0, 2.0 * R) + (uEye - vec3(R));
    wpos.y += sin(uTime * 0.6 + aSeed.w * 6.2831) * 0.4; // gentle bob

    gl_Position = persp * view * vec4(wpos, 1.0);
    float dist = length(wpos - uEye);
    gl_PointSize = clamp(uSizePx / max(dist, 1.0), 1.0, 16.0);
    vFade = 1.0 - smoothstep(R * 0.45, R, dist); // fade out at the box edge
}
