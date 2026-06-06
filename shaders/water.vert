#version 330 core
// The Mere: a flat water plane at y = 0. Rendered after lighting, depth-tested
// against the scene so it fills the central basin (and is hidden under the land).
layout (location = 0) in vec3 inPos;
uniform mat4 persp;
uniform mat4 view;
out vec3 vWorldPos;
void main() {
    vWorldPos = inPos;
    gl_Position = persp * view * vec4(inPos, 1.0);
}
