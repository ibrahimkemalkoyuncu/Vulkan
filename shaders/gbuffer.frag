#version 450
// G-Buffer fragment shader — writes color, view-space position, view-space normal

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragViewPos;
layout(location = 2) in vec3 fragViewNormal;

layout(location = 0) out vec4 gColor;    // RGBA8_UNORM  scene color
layout(location = 1) out vec4 gPosition; // RGBA32F      view-space position
layout(location = 2) out vec4 gNormal;   // RGBA16F      view-space normal (encoded 0..1)

void main() {
    gColor    = vec4(fragColor, 1.0);
    gPosition = vec4(fragViewPos, 1.0);
    // Pack normal from [-1,1] into [0,1] for storage
    gNormal   = vec4(normalize(fragViewNormal) * 0.5 + 0.5, 1.0);
}
