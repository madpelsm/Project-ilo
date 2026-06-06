#version 330 core
// 4x4 box blur over the raw AO buffer, matching the 4x4 noise tile so the per-pixel
// rotation dither averages away into smooth contact shadow.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uAO;
uniform vec2 uTexel; // 1 / AO-buffer size

void main() {
    float sum = 0.0;
    for (int x = -2; x < 2; x++)
        for (int y = -2; y < 2; y++)
            sum += texture(uAO, TexCoords + vec2(float(x), float(y)) * uTexel).r;
    FragColor = vec4(sum / 16.0, 0.0, 0.0, 1.0);
}
