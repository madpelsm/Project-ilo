#version 330 core
in float vFade;
out vec3 FragColor;
uniform vec3 uColor;
void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r = dot(c, c);
    if (r > 1.0)
        discard;
    float a = (1.0 - r);
    a *= a; // soft round falloff
    FragColor = uColor * a * vFade;
}
