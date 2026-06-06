#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inMtlProps;          // shininess, specStrength, ambient
layout (location = 4) in vec3 instanceOffset;
layout (location = 5) in vec4 instanceTintEmissive; // rgb = colour tint, a = emissive strength
layout (location = 6) in vec4 instanceXform;        // x=scale, y=yaw, z=windStiffness, w=phase

uniform mat4 persp;
uniform mat4 model;
uniform mat4 view;
uniform mat3 normalMatrix; // transpose(inverse(mat3(model))), computed once on the CPU
uniform vec3 uOriginOffset; // floating-origin: G-buffer stores positions relative to this
uniform vec2 uWind;         // wind direction * strength
uniform float time;
uniform float grassWave;   // 1.0 for the forest (animate low verts), 0.0 otherwise

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vAlbedo;
out vec3 vMtlProps;
out float vEmissive;

void main() {
    vec3 vertPos = inPosition;
    // Gentle grass/undergrowth sway: only the lowest geometry, only the forest.
    if (grassWave > 0.5 && inPosition.y < 0.1) {
        vertPos.y += 0.2 * sin(time + inPosition.x + inPosition.z);
    }

    // Per-instance scale + yaw (loc6 defaults to scale 1 / no rotation for non-fields).
    float yaw = instanceXform.y;
    float cy = cos(yaw), sy = sin(yaw);
    vec3 p = vertPos * instanceXform.x;
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);

    // Wind: bend the upper part of an instance in the wind direction; tops sway more.
    float stiff = instanceXform.z;
    float h = max(vertPos.y, 0.0) * instanceXform.x;
    float gust = 0.65 + 0.35 * sin(time * 0.6 + (instanceOffset.x + instanceOffset.z) * 0.04);
    float sway = stiff * h * gust * sin(time * 1.7 + instanceXform.w +
                                        instanceOffset.x * 0.12 + instanceOffset.z * 0.12);
    p.x += sway * uWind.x;
    p.z += sway * uWind.y;

    vec4 worldPos = model * vec4(p + instanceOffset, 1.0);
    gl_Position = persp * view * worldPos;

    // Store position relative to a nearby snapped origin so the fp16 G-buffer stays
    // precise no matter how far the player roams from the world centre.
    vWorldPos = worldPos.xyz - uOriginOffset;
    vec3 nrm = vec3(cy * inNormal.x + sy * inNormal.z, inNormal.y, -sy * inNormal.x + cy * inNormal.z);
    vNormal = normalMatrix * nrm;
    vAlbedo = inColor * instanceTintEmissive.rgb;
    vMtlProps = inMtlProps;
    vEmissive = instanceTintEmissive.a;
}
