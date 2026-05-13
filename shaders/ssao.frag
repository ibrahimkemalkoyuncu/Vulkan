#version 450
// Screen-Space Ambient Occlusion — hemisphere kernel, noise-based TBN rotation

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outOcclusion; // .r = AO factor in [0,1]

layout(set = 0, binding = 0) uniform sampler2D gPosition; // view-space xyz
layout(set = 0, binding = 1) uniform sampler2D gNormal;   // packed normal
layout(set = 0, binding = 2) uniform sampler2D texNoise;  // 4x4 random vectors

layout(set = 1, binding = 0) uniform KernelUBO {
    vec4 samples[64]; // hemisphere kernel, xyz used
} kernel;

layout(push_constant) uniform SSAOParams {
    mat4  projection;
    float radius;
    float bias;
    float power;
    float noiseScaleX; // screenWidth  / 4.0
    float noiseScaleY; // screenHeight / 4.0
} params;

void main() {
    vec3 fragPos = texture(gPosition, fragUV).xyz;
    vec3 normal  = texture(gNormal,   fragUV).xyz * 2.0 - 1.0;

    // Background pixels or line fragments with no surface normal → no occlusion
    if (fragPos.z == 0.0 || dot(normal, normal) < 0.01) {
        outOcclusion = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }
    normal = normalize(normal);

    vec2  noiseUV  = fragUV * vec2(params.noiseScaleX, params.noiseScaleY);
    vec3  randVec  = normalize(texture(texNoise, noiseUV).xyz * 2.0 - 1.0);

    // Build TBN to orient hemisphere kernel to the surface normal
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i) {
        // Orient kernel sample into view space
        vec3 samplePos = TBN * kernel.samples[i].xyz;
        samplePos = fragPos + samplePos * params.radius;

        // Project to texture coordinates
        vec4 offset  = params.projection * vec4(samplePos, 1.0);
        offset.xy   /= offset.w;
        offset.xy    = offset.xy * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 ||
            offset.y < 0.0 || offset.y > 1.0) continue;

        float sampleDepth = texture(gPosition, offset.xy).z;

        // Range-check: fade contribution to 0 as depth diff approaches radius
        // (prevents far surfaces from incorrectly darkening closer geometry)
        float rangeCheck = smoothstep(0.0, 1.0,
            1.0 - abs(fragPos.z - sampleDepth) / params.radius);

        // Occluded if the G-buffer sample is in front of or at our sample point
        occlusion += (sampleDepth >= samplePos.z + params.bias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = pow(1.0 - (occlusion / 64.0), params.power);
    outOcclusion = vec4(ao, 0.0, 0.0, 1.0);
}
