#version 330 core
// Screen-space reflections for the wet shoreline only (gMtlProps.a wetness). Marches the
// reflected view ray through the G-buffer depth and samples the lit HDR scene on a hit;
// on a miss (rays off near-flat ground leave the frame) it falls back to a horizon-sky
// tint so the wet band still reads glossy rather than black. Output: rgb radiance,
// a = blend weight (fresnel * wetness * confidence, capped). Origin-relative, projecting
// march samples with the same uViewProjRel the SSAO pass uses.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D gPosition; // origin-relative world pos
uniform sampler2D gNormal;
uniform sampler2D gMtlProps; // .a = wetness
uniform sampler2D uHdrScene; // the lit opaque scene (fog/mist baked in)

uniform vec3 eyePos;        // origin-relative
uniform mat4 uViewProjRel;  // origin-relative world -> clip (== SSAO's)
uniform vec3 uHorizonTint;  // miss fallback (sky horizon colour)
uniform int uSSRSteps;
uniform float uSSRStride;
uniform float uSSRThickness;

void main() {
    float wet = texture(gMtlProps, TexCoords).a;
    vec3 N = texture(gNormal, TexCoords).rgb;
    if (wet < 0.02 || dot(N, N) < 0.25) {
        FragColor = vec4(0.0);
        return;
    }
    N = normalize(N);
    vec3 P = texture(gPosition, TexCoords).rgb;
    vec3 V = normalize(P - eyePos);     // eye -> fragment
    vec3 R = reflect(V, N);             // reflected ray
    float fres = 0.05 + 0.95 * pow(1.0 - max(dot(-V, N), 0.0), 5.0);

    vec3 pos = P + N * 0.06; // lift off the surface to avoid self-hit acne
    vec3 step = R * uSSRStride;
    float hit = 0.0;
    vec3 hitCol = vec3(0.0);
    for (int i = 0; i < 48; i++) {
        if (i >= uSSRSteps)
            break;
        pos += step;
        vec4 clip = uViewProjRel * vec4(pos, 1.0);
        if (clip.w <= 0.0)
            break;
        vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
            break;
        vec3 scenePos = texture(gPosition, suv).rgb;
        float dRay = length(pos - eyePos);
        float dScene = length(scenePos - eyePos);
        if (dScene < dRay && dRay - dScene < uSSRThickness) {
            hitCol = texture(uHdrScene, suv).rgb;
            // fade as the hit nears the screen edge (avoids hard cutoffs)
            vec2 e = abs(suv * 2.0 - 1.0);
            hit = (1.0 - smoothstep(0.7, 1.0, max(e.x, e.y)));
            break;
        }
    }
    vec3 col = mix(uHorizonTint, hitCol, hit);   // miss -> horizon sky sheen
    float conf = mix(0.45, 1.0, hit);            // weaker for the fallback
    FragColor = vec4(col, clamp(fres * wet * conf * 0.6, 0.0, 0.55));
}
