#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inMtlProps;          // shininess, specStrength, ambient
layout (location = 4) in vec3 instanceOffset;
layout (location = 5) in vec4 instanceTintEmissive; // rgb = colour tint, a = emissive strength

uniform mat4 persp;
uniform mat4 model;
uniform mat4 view;
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

    vec4 worldPos = model * vec4(vertPos + instanceOffset, 1.0);
    gl_Position = persp * view * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(model))) * inNormal;
    vAlbedo = inColor * instanceTintEmissive.rgb;
    vMtlProps = inMtlProps;
    vEmissive = instanceTintEmissive.a;
}
