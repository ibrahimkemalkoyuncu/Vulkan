#version 450
// Fullscreen triangle — no vertex buffer needed, driven by gl_VertexIndex (0,1,2)

layout(location = 0) out vec2 fragUV;

void main() {
    // NDC triangle that covers the entire screen in one draw call (vkCmdDraw(3,1,0,0))
    vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    fragUV      = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
