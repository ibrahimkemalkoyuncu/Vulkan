/**
 * @file test_dwg_colors.cpp
 * @brief DWG renk analizi — fix sonrası doğrulama
 */

#include <catch2/catch_test_macros.hpp>
#include "cad/DWGReader.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

using namespace vkt::cad;

TEST_CASE("DWG color distribution after fix", "[dwg][color]") {
    DWGReader reader;
    const std::string path = "C:/Users/afney/Desktop/AutoCad Dosya/ornek_proje.dwg";

    bool ok = reader.Read(path);
    REQUIRE(ok);

    auto entities = reader.TakeEntities();
    auto layers   = reader.GetLayers();

    std::cout << "\n=== DWG Color Analysis (After Fix) ===" << std::endl;
    std::cout << "Total entities: " << entities.size() << std::endl;
    std::cout << "Total layers:   " << layers.size() << std::endl;

    // Layer renk dağılımı
    int whiteLayerCount = 0;
    std::cout << "\n--- Layer Colors ---" << std::endl;
    for (const auto& [name, layer] : layers) {
        const auto& c = layer.GetColor();
        bool isWhite = (c.r == 255 && c.g == 255 && c.b == 255);
        if (isWhite) whiteLayerCount++;
        std::cout << "  '" << name << "' => RGB("
                  << (int)c.r << "," << (int)c.g << "," << (int)c.b << ")"
                  << (isWhite ? " [WHITE]" : "") << std::endl;
    }

    // Entity renk analizi
    int byLayer = 0, byBlock = 0, explicitWhite = 0, explicitColored = 0;
    int layerMiss = 0;
    std::map<std::string, int> entityLayerCount;

    for (const auto& e : entities) {
        if (!e) continue;
        Color col = e->GetColor();
        const std::string& lname = e->GetLayerName();
        entityLayerCount[lname]++;

        if (col.IsByLayer()) {
            byLayer++;
            if (layers.find(lname) == layers.end()) layerMiss++;
        } else if (col.IsByBlock()) {
            byBlock++;
        } else {
            if (col.r == 255 && col.g == 255 && col.b == 255) explicitWhite++;
            else explicitColored++;
        }
    }

    std::cout << "\n--- Entity Color Stats ---" << std::endl;
    std::cout << "  ByLayer:        " << byLayer        << std::endl;
    std::cout << "  ByBlock:        " << byBlock        << std::endl;
    std::cout << "  Explicit White: " << explicitWhite  << std::endl;
    std::cout << "  Explicit Color: " << explicitColored<< std::endl;
    std::cout << "  Layer Miss:     " << layerMiss      << std::endl;
    std::cout << "  White layers:   " << whiteLayerCount << "/" << layers.size() << std::endl;

    // Top layers by entity count
    std::cout << "\n--- Top Entity Layers ---" << std::endl;
    std::vector<std::pair<int,std::string>> sorted;
    for (const auto& [n, c] : entityLayerCount) sorted.push_back({c, n});
    std::sort(sorted.rbegin(), sorted.rend());
    for (int i = 0; i < std::min(10, (int)sorted.size()); ++i) {
        const auto& [cnt, name] = sorted[i];
        auto it = layers.find(name);
        std::string colorStr = "MISSING";
        if (it != layers.end()) {
            const auto& c = it->second.GetColor();
            colorStr = "RGB(" + std::to_string((int)c.r) + "," +
                       std::to_string((int)c.g) + "," +
                       std::to_string((int)c.b) + ")";
        }
        std::cout << "  " << cnt << "x  '" << name << "' => " << colorStr << std::endl;
    }

    REQUIRE(entities.size() > 0);
    REQUIRE(whiteLayerCount < (int)layers.size()); // en az bir renkli layer olmalı
}
