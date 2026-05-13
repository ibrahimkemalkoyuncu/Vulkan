#version 450
// G-Buffer vertex shader — outputs view-space position + normal alongside color

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;        // clip-space transform
    mat4 modelView;  // view-space transform (128 bytes total — at Vulkan minspec)
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragViewPos;
layout(location = 2) out vec3 fragViewNormal;

void main() {
    vec4 viewPos     = pc.modelView * vec4(inPos, 1.0);
    gl_Position      = pc.mvp * vec4(inPos, 1.0);
    fragColor        = inColor;
    fragViewPos      = viewPos.xyz;
    fragViewNormal   = normalize(mat3(pc.modelView) * inNormal);
}
