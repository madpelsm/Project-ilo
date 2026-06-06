#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTex; // font atlas (R8) for text, or 1x1 white for solid bars
void main() {
    float a = texture(uTex, vUV).r;
    FragColor = vec4(vColor.rgb, vColor.a * a);
    // HUD is drawn after gamma encoding, so colours are treated as display-space.
}
