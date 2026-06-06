#version 330 core
// Separable 5-tap Gaussian blur (run horizontally then vertically).
in vec2 TexCoords;
out vec3 FragColor;
uniform sampler2D image;
uniform bool horizontal;
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texel = 1.0 / vec2(textureSize(image, 0)); // sample by the source's own texel size
    vec3 result = texture(image, TexCoords).rgb * weight[0];
    for (int i = 1; i < 5; ++i) {
        vec2 o = horizontal ? vec2(texel.x * float(i), 0.0) : vec2(0.0, texel.y * float(i));
        result += texture(image, TexCoords + o).rgb * weight[i];
        result += texture(image, TexCoords - o).rgb * weight[i];
    }
    FragColor = result;
}
