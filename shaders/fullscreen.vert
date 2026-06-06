#version 330 core
// Generates a single screen-covering triangle from gl_VertexID, no VBO needed.
out vec2 TexCoords;
void main() {
    vec2 p = vec2((gl_VertexID == 2) ? 3.0 : -1.0,
                  (gl_VertexID == 1) ? 3.0 : -1.0);
    TexCoords = (p + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
