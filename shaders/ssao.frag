#version 330 core
// Screen-space ambient occlusion. Reads the deferred G-buffer (origin-relative world
// position + world normal), gathers a hemisphere of samples oriented to the surface,
// and darkens creases / contact points where geometry crowds in. Output is a single
// occlusion factor (1 = open, 0 = fully occluded) consumed by the lighting pass to
// ground the low-poly forms instead of letting them float on flat fill light.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D gPosition; // rgb = world pos relative to the floating origin
uniform sampler2D gNormal;   // rgb = world normal

uniform vec3 eyePos;       // camera, in the same origin-relative space
uniform mat4 uViewProjRel; // proj * view * translate(origin): origin-relative world -> clip
uniform vec3 uKernel[16];  // hemisphere sample offsets (tangent space, +z = normal)
uniform vec3 uNoise[16];   // 4x4 tile of random tangent-plane rotations
uniform float uRadius;     // sampling radius in metres
uniform float uBias;       // depth bias to kill self-occlusion acne
uniform float uPower;      // contrast of the occlusion falloff

void main() {
    vec3 N = texture(gNormal, TexCoords).rgb;
    if (dot(N, N) < 0.25) { // background sky: nothing to occlude
        FragColor = vec4(1.0);
        return;
    }
    N = normalize(N);
    vec3 P = texture(gPosition, TexCoords).rgb;

    // Per-pixel rotated tangent basis from the tiled noise (Gram-Schmidt).
    ivec2 px = ivec2(gl_FragCoord.xy);
    vec3 randv = uNoise[(px.x & 3) * 4 + (px.y & 3)];
    vec3 T = normalize(randv - N * dot(randv, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float fragDist = length(P - eyePos);
    float occ = 0.0;
    for (int i = 0; i < 16; i++) {
        vec3 sp = P + (TBN * uKernel[i]) * uRadius;
        vec4 clip = uViewProjRel * vec4(sp, 1.0);
        if (clip.w <= 0.0)
            continue;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            continue;
        vec3 gp = texture(gPosition, uv).rgb;
        float sampleDist = length(sp - eyePos);
        float geomDist = length(gp - eyePos);
        // Only count occluders within the radius shell so distant geometry behind the
        // sample doesn't bleed dark halos across silhouettes.
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / max(abs(fragDist - geomDist), 1e-4));
        occ += (geomDist <= sampleDist - uBias ? 1.0 : 0.0) * rangeCheck;
    }
    float ao = 1.0 - occ / 16.0;
    FragColor = vec4(pow(clamp(ao, 0.0, 1.0), uPower), 0.0, 0.0, 1.0);
}
