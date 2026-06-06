#version 330 core
// Crepuscular rays: march from each pixel toward the sun's screen position, summing
// sky brightness (occluded by geometry via the G-buffer normal/sky mask). Quarter-res,
// added into the HDR scene before bloom so the shafts glow. Strong at dawn/dusk.
in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D uHdr;    // the lit scene (sky lives here in the background)
uniform sampler2D gNormal; // sky where dot(N,N) < 0.25
uniform vec2 uSunScreen;   // sun position in [0,1]
uniform float uStrength;   // 0 when the sun is below the horizon / off screen
uniform vec3 uSunColor;

void main() {
    vec2 dir = uSunScreen - TexCoords;
    const int N = 24;
    vec2 stp = dir / float(N) * 0.92;
    vec2 s = TexCoords;
    float w = 0.5, decay = 0.95, accum = 0.0;
    for (int i = 0; i < N; i++) {
        s += stp;
        vec3 n = texture(gNormal, s).rgb;
        float isSky = (dot(n, n) < 0.25) ? 1.0 : 0.0;
        vec3 c = texture(uHdr, s).rgb;
        accum += isSky * dot(c, vec3(0.3, 0.6, 0.1)) * w;
        w *= decay;
    }
    accum /= float(N);
    FragColor = uSunColor * accum * uStrength;
}
