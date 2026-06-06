#version 330 core
// Geometry pass: write the G-buffer (4 MRT). Emissive strength is packed in
// the position target's alpha; emissive colour is reconstructed as albedo*strength.
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vAlbedo;
in vec3 vMtlProps;
in float vEmissive;

layout (location = 0) out vec4 oPosition; // rgb = world pos, a = emissive strength
layout (location = 1) out vec4 oNormal;   // rgb = normal,    a = 1 (reserved)
layout (location = 2) out vec4 oAlbedo;   // rgb = albedo,    a = 1
layout (location = 3) out vec4 oMtlProps; // r = shininess/256, g = spec, b = ambient, a = flags

void main() {
    oPosition = vec4(vWorldPos, vEmissive);
    oNormal = vec4(normalize(vNormal), 1.0);
    oAlbedo = vec4(vAlbedo, 1.0);
    // .a = shoreline wetness: a thin up-facing band just above the Mere (vWorldPos.y is
    // true world Y since the floating origin only snaps X/Z). Read only by the SSR pass.
    float wet = smoothstep(1.6, 0.0, vWorldPos.y) * smoothstep(-0.2, 0.05, vWorldPos.y) *
                smoothstep(0.4, 0.85, clamp(normalize(vNormal).y, 0.0, 1.0));
    oMtlProps = vec4(vMtlProps.x / 256.0, vMtlProps.y, vMtlProps.z, wet);
}
