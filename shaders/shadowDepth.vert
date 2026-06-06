#version 330 core
// Sun shadow-map depth pass. The vertex transform here MUST stay byte-for-byte in
// sync with shaders/firstPassVertex.vert (per-instance scale/yaw + grass-wave + the
// wind cantilever bend) so a swaying caster's shadow stays welded to the caster.
// Outputs only clip position in the sun's light space; the fragment shader is empty.
// KEEP IN SYNC WITH firstPassVertex.vert.
layout (location = 0) in vec3 inPosition;
layout (location = 4) in vec3 instanceOffset;
layout (location = 6) in vec4 instanceXform; // x=scale, y=yaw, z=windStiffness, w=phase

uniform mat4 model;
uniform mat4 uLightVP;      // origin-relative world -> sun light clip
uniform vec3 uOriginOffset; // floating origin (same value the G-buffer subtracts)
uniform vec2 uWind;
uniform float time;
uniform float grassWave;

void main() {
    vec3 vertPos = inPosition;
    if (grassWave > 0.5 && inPosition.y < 0.1) {
        vertPos.y += 0.2 * sin(time + inPosition.x + inPosition.z);
    }
    float yaw = instanceXform.y;
    float cy = cos(yaw), sy = sin(yaw);
    vec3 p = vertPos * instanceXform.x;
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);

    float stiff = instanceXform.z;
    float ph = instanceXform.w;
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
    gl_Position = uLightVP * vec4(worldPos.xyz - uOriginOffset, 1.0);
}
