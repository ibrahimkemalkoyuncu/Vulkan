#version 450
// 4×4 box blur to remove SSAO noise

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D ssaoInput;

void main() {
    vec2  texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    int   count  = 0;
    // 4×4 kernel centered at the current fragment (half-texel offsets: -1.5 … +1.5)
    // Skip background pixels (ao == 1.0) to prevent AO bleeding at geometry edges
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            vec2  offset = vec2(float(x) - 1.5, float(y) - 1.5) * texelSize;
            float s      = texture(ssaoInput, fragUV + offset).r;
            if (s < 0.999) { result += s; ++count; }
        }
    }
    float ao = (count > 0) ? (result / float(count)) : 1.0;
    outColor = vec4(ao, 0.0, 0.0, 1.0);
}
