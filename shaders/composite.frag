#version 450
// PBR composite: GGX Cook-Torrance specular + Lambert diffuse + SSAO
// Uses G-Buffer data: albedo (binding=0), ssaoBlur (binding=1),
//                    gNormal (binding=2, view-space packed), gPosition (binding=3)

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D ssaoBlur;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gPosition;

layout(push_constant) uniform PC {
    vec4  lightDir;   // view-space light direction (normalized, w unused)
    float roughness;
    float metalness;
    float ambient;
    float pad;
} pc;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float alpha) {
    float a2    = alpha * alpha;
    float NdH   = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdX, float k) {
    return NdX / (NdX * (1.0 - k) + k);
}

float GeometrySmith(float NdV, float NdL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return GeometrySchlickGGX(NdV, k) * GeometrySchlickGGX(NdL, k);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3  albedo  = texture(gAlbedo,   fragUV).rgb;
    float ao      = texture(ssaoBlur,  fragUV).r;
    vec3  normPkd = texture(gNormal,   fragUV).rgb;
    vec3  viewPos = texture(gPosition, fragUV).xyz;

    // If position == 0 (background pixel), output ambient-only
    if (dot(viewPos, viewPos) < 1e-6) {
        outColor = vec4(albedo * pc.ambient, 1.0);
        return;
    }

    vec3 N = normalize(normPkd * 2.0 - 1.0);   // unpack view-space normal
    vec3 V = normalize(-viewPos);               // view-space: camera at origin
    vec3 L = normalize(pc.lightDir.xyz);
    vec3 H = normalize(V + L);

    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 0.001);

    vec3 F0 = mix(vec3(0.04), albedo, pc.metalness);
    vec3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float D = DistributionGGX(N, H, pc.roughness * pc.roughness);
    float G = GeometrySmith(NdV, NdL, pc.roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdV * NdL, 0.001);
    vec3 kD       = (1.0 - F) * (1.0 - pc.metalness);
    vec3 diffuse  = kD * albedo / PI;

    vec3 Lo      = (diffuse + specular) * NdL;
    vec3 ambient = albedo * pc.ambient * ao;
    vec3 color   = ambient + Lo;

    // Reinhard + gamma
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
