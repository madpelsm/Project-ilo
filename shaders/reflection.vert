#version 330 core
// Planar reflection of the world in the Mere: the scene mirrored about y=0 into a small
// reflection target. The vertex transform MUST match firstPassVertex.vert (per-instance
// scale/yaw + grass-wave + wind bend); ONLY the clip projection is mirrored (uReflVP).
// The shading varyings use the UN-mirrored world so the reflected geometry is lit exactly
// like the real geometry. KEEP IN SYNC WITH firstPassVertex.vert.
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inMtlProps;
layout (location = 4) in vec3 instanceOffset;
layout (location = 5) in vec4 instanceTintEmissive;
layout (location = 6) in vec4 instanceXform;

uniform mat4 model;
uniform mat3 normalMatrix;
uniform mat4 uReflVP;       // projection * view * mirror(y) — origin handled below
uniform vec3 uOriginOffset;
uniform vec2 uWind;
uniform float time;
uniform float grassWave;

out vec3 vPosRel;
out vec3 vNormal;
out vec3 vAlbedo;
out vec3 vMtlProps;
out float vEmissive;
out float vWorldY;

void main() {
    vec3 vertPos = inPosition;
    if (grassWave > 0.5 && inPosition.y < 0.1)
        vertPos.y += 0.2 * sin(time + inPosition.x + inPosition.z);
    float yaw = instanceXform.y;
    float cy = cos(yaw), sy = sin(yaw);
    vec3 p = vertPos * instanceXform.x;
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
    float stiff = instanceXform.z, ph = instanceXform.w;
    vec2 wdir = length(uWind) > 1e-4 ? normalize(uWind) : vec2(1.0, 0.0);
    float str = length(uWind);
    float h = max(vertPos.y, 0.0) * instanceXform.x;
    float along = dot(instanceOffset.xz, wdir);
    float gust = sin(along * 0.05 - time * 0.9 + ph) * 0.60 +
                 sin(along * 0.17 - time * 1.7 + ph * 1.7) * 0.30 +
                 sin(time * 3.1 + ph * 2.3) * 0.10;
    float bend = stiff * str * h * (0.55 + 0.45 * gust);
    p.x += wdir.x * bend;
    p.z += wdir.y * bend;
    p.y -= 0.5 * bend * bend / max(h, 0.05);

    vec4 worldPos = model * vec4(p + instanceOffset, 1.0);
    gl_Position = uReflVP * worldPos; // mirror lives in uReflVP
    vPosRel = worldPos.xyz - uOriginOffset;
    vWorldY = worldPos.y;
    vec3 nrm = vec3(cy * inNormal.x + sy * inNormal.z, inNormal.y, -sy * inNormal.x + cy * inNormal.z);
    vNormal = normalMatrix * nrm;
    vAlbedo = inColor * instanceTintEmissive.rgb;
    vMtlProps = inMtlProps;
    vEmissive = instanceTintEmissive.a;
}
