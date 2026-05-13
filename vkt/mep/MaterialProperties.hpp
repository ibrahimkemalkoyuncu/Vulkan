#pragma once
#include <string>
#include <unordered_map>

namespace vkt::mep {

struct MaterialProperties {
    float baseColor[3] = {0.8f, 0.8f, 0.8f}; // linear-space RGB albedo
    float metallic     = 0.0f;
    float roughness    = 0.5f;
    float emission[3]  = {0.0f, 0.0f, 0.0f};
};

// Returns PBR properties for a given pipe material name.
inline MaterialProperties GetPipeMaterial(const std::string& name) {
    static const std::unordered_map<std::string, MaterialProperties> kTable = {
        {"Bakır",        {{0.72f, 0.45f, 0.20f}, 0.90f, 0.30f, {}}},
        {"Çelik",        {{0.56f, 0.57f, 0.58f}, 0.88f, 0.40f, {}}},
        {"Paslanmaz",    {{0.65f, 0.65f, 0.65f}, 0.90f, 0.25f, {}}},
        {"Dökme Demir",  {{0.30f, 0.30f, 0.30f}, 0.50f, 0.80f, {}}},
        {"PPR",          {{0.90f, 0.90f, 0.95f}, 0.00f, 0.65f, {}}},
        {"PVC",          {{0.85f, 0.85f, 0.85f}, 0.00f, 0.70f, {}}},
        {"HDPE",         {{0.30f, 0.50f, 0.30f}, 0.00f, 0.60f, {}}},
        {"Galvaniz",     {{0.70f, 0.70f, 0.65f}, 0.70f, 0.55f, {}}},
    };
    auto it = kTable.find(name);
    return it != kTable.end() ? it->second : MaterialProperties{};
}

} // namespace vkt::mep
