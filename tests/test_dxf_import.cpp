/**
 * @file test_dxf_import.cpp
 * @brief DXF import testleri — in-memory DXF string ile parse doğrulaması
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "cad/DXFReader.hpp"
#include "cad/Entity.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Polyline.hpp"
#include "cad/LayerManager.hpp"
#include "cad/SpaceManager.hpp"
#include "cad/Text.hpp"
#include <fstream>
#include <filesystem>
#include <string>

using namespace vkt::cad;
using namespace vkt::geom;
using Catch::Matchers::WithinAbs;

// Helper: geçici DXF dosyası oluştur ve path döndür
// Raw string literal'lerin başındaki boş satırı (R"(\n...) ) atlar.
static std::string WriteTempDXF(const std::string& content) {
    auto path = (std::filesystem::temp_directory_path() / "vkt_test.dxf").string();
    std::ofstream f(path);
    const char* start = content.c_str();
    while (*start == '\n' || *start == '\r') ++start;
    f << start;
    return path;
}

// Minimal valid DXF with a single LINE
static constexpr const char* DXF_SINGLE_LINE = R"(
  0
SECTION
  2
ENTITIES
  0
LINE
  8
TEST_LAYER
 10
0.0
 20
0.0
 30
0.0
 11
3.0
 21
4.0
 31
0.0
  0
ENDSEC
  0
EOF
)";

// DXF with one CIRCLE
static constexpr const char* DXF_CIRCLE = R"(
  0
SECTION
  2
ENTITIES
  0
CIRCLE
  8
CIRCLES
 10
5.0
 20
5.0
 30
0.0
 40
3.0
  0
ENDSEC
  0
EOF
)";

// DXF with a closed LWPOLYLINE (unit square)
static constexpr const char* DXF_LWPOLY = R"(
  0
SECTION
  2
ENTITIES
  0
LWPOLYLINE
  8
WALLS
 90
4
 70
1
 10
0.0
 20
0.0
 10
1.0
 20
0.0
 10
1.0
 20
1.0
 10
0.0
 20
1.0
  0
ENDSEC
  0
EOF
)";

// ============================================================

TEST_CASE("DXFReader parses a single LINE entity", "[dxf]") {
    std::string path = WriteTempDXF(DXF_SINGLE_LINE);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    const auto& entities = reader.GetEntities();
    REQUIRE(entities.size() == 1);
    REQUIRE(entities[0]->GetType() == EntityType::Line);

    Line* line = static_cast<Line*>(entities[0].get());
    REQUIRE_THAT(line->GetLength(), WithinAbs(5.0, 1e-6)); // 3-4-5 triangle

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader parses CIRCLE entity", "[dxf]") {
    std::string path = WriteTempDXF(DXF_CIRCLE);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    const auto& entities = reader.GetEntities();
    REQUIRE(entities.size() == 1);
    REQUIRE(entities[0]->GetType() == EntityType::Circle);

    Circle* circle = static_cast<Circle*>(entities[0].get());
    REQUIRE_THAT(circle->GetRadius(),   WithinAbs(3.0, 1e-6));
    REQUIRE_THAT(circle->GetCenter().x, WithinAbs(5.0, 1e-6));
    REQUIRE_THAT(circle->GetCenter().y, WithinAbs(5.0, 1e-6));

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader parses closed LWPOLYLINE", "[dxf]") {
    std::string path = WriteTempDXF(DXF_LWPOLY);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    const auto& entities = reader.GetEntities();
    REQUIRE(entities.size() >= 1);

    bool foundPoly = false;
    for (const auto& e : entities) {
        if (e->GetType() == EntityType::Polyline) {
            Polyline* poly = static_cast<Polyline*>(e.get());
            REQUIRE(poly->IsClosed());
            REQUIRE(poly->GetVertexCount() == 4);
            foundPoly = true;
            break;
        }
    }
    REQUIRE(foundPoly);

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader layer filter includes matching entities", "[dxf]") {
    std::string path = WriteTempDXF(DXF_SINGLE_LINE);
    DXFReader reader(path);
    reader.FilterByLayer({"TEST_LAYER"});
    REQUIRE(reader.Read());

    auto filtered = reader.GetFilteredEntities();
    REQUIRE(filtered.size() == 1);

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader layer filter excludes non-matching entities", "[dxf]") {
    std::string path = WriteTempDXF(DXF_SINGLE_LINE);
    DXFReader reader(path);
    reader.FilterByLayer({"WRONG_LAYER"});
    REQUIRE(reader.Read());

    auto filtered = reader.GetFilteredEntities();
    REQUIRE(filtered.empty());

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader ImportLayersTo populates LayerManager", "[dxf]") {
    std::string path = WriteTempDXF(DXF_SINGLE_LINE);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    LayerManager lm;
    reader.ImportLayersTo(lm);
    REQUIRE(lm.HasLayer("TEST_LAYER"));

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader GetStatistics reflects parsed entities", "[dxf]") {
    std::string path = WriteTempDXF(DXF_SINGLE_LINE);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    auto stats = reader.GetStatistics();
    REQUIRE(stats.totalEntities >= 1);

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader returns false for non-existent file", "[dxf]") {
    DXFReader reader("/nonexistent/path/file.dxf");
    REQUIRE_FALSE(reader.Read());
}

// DXF with $INSUNITS = 4 (millimeters) in HEADER section
static constexpr const char* DXF_WITH_INSUNITS_MM = R"(
  0
SECTION
  2
HEADER
  9
$INSUNITS
 70
4
  0
ENDSEC
  0
SECTION
  2
ENTITIES
  0
ENDSEC
  0
EOF
)";

static constexpr const char* DXF_WITH_INSUNITS_M = R"(
  0
SECTION
  2
HEADER
  9
$INSUNITS
 70
6
  0
ENDSEC
  0
SECTION
  2
ENTITIES
  0
ENDSEC
  0
EOF
)";

TEST_CASE("DXFReader parses $INSUNITS millimeters (4)", "[dxf][header]") {
    std::string path = WriteTempDXF(DXF_WITH_INSUNITS_MM);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    REQUIRE(reader.GetHeader().insUnits == 4);
    REQUIRE_THAT(reader.GetHeader().GetScaleToMM(),  WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(reader.GetHeader().GetAreaToM2(),   WithinAbs(1e-6, 1e-15));

    std::filesystem::remove(path);
}

TEST_CASE("DXFReader parses $INSUNITS meters (6)", "[dxf][header]") {
    std::string path = WriteTempDXF(DXF_WITH_INSUNITS_M);
    DXFReader reader(path);
    REQUIRE(reader.Read());

    REQUIRE(reader.GetHeader().insUnits == 6);
    REQUIRE_THAT(reader.GetHeader().GetScaleToMM(), WithinAbs(1000.0, 1e-9));
    REQUIRE_THAT(reader.GetHeader().GetAreaToM2(),  WithinAbs(1.0, 1e-9));

    std::filesystem::remove(path);
}

// ============================================================
// ValidateAll / GetInvalidSpaces
// ============================================================

static vkt::cad::Polyline* makeRoomPoly(
    std::vector<std::unique_ptr<vkt::cad::Polyline>>& store,
    double w, double h)
{
    using namespace vkt::geom;
    auto p = std::make_unique<vkt::cad::Polyline>(
        std::vector<Vec3>{{0,0,0},{w,0,0},{w,h,0},{0,h,0}}, true);
    store.push_back(std::move(p));
    return store.back().get();
}

TEST_CASE("SpaceManager ValidateAll — all valid spaces", "[space][validate]") {
    using namespace vkt::cad;
    std::vector<std::unique_ptr<Polyline>> store;

    SpaceManager mgr;
    // 3m×3m = 9 m² (geçerli)
    mgr.CreateSpace(makeRoomPoly(store, 3.0, 3.0), "Oda 1");
    mgr.CreateSpace(makeRoomPoly(store, 4.0, 5.0), "Oda 2");

    REQUIRE(mgr.ValidateAll() == 0);
    REQUIRE(mgr.GetInvalidSpaces().empty());

    auto stats = mgr.GetStatistics();
    REQUIRE(stats.totalSpaces == 2);
    REQUIRE(stats.validSpaces == 2);
    REQUIRE(stats.invalidSpaces == 0);
}

TEST_CASE("SpaceManager ValidateAll — detects invalid (tiny) space", "[space][validate]") {
    using namespace vkt::cad;
    std::vector<std::unique_ptr<Polyline>> store;

    SpaceManager mgr;
    mgr.CreateSpace(makeRoomPoly(store, 3.0, 3.0), "Büyük Oda"); // geçerli
    mgr.CreateSpace(makeRoomPoly(store, 0.3, 0.3), "Küçük Oda"); // 0.09 m² < 0.5 → geçersiz

    REQUIRE(mgr.ValidateAll() == 1);
    auto invalid = mgr.GetInvalidSpaces();
    REQUIRE(invalid.size() == 1);
    REQUIRE(invalid[0]->GetName() == "Küçük Oda");

    auto stats = mgr.GetStatistics();
    REQUIRE(stats.validSpaces == 1);
    REQUIRE(stats.invalidSpaces == 1);
}

// ============================================================
// DetectNameFromText — boyut filtresi
// ============================================================

TEST_CASE("SpaceManager DetectSpacesFromEntities — dimension text ignored", "[space][text]") {
    using namespace vkt::cad;
    using namespace vkt::geom;

    // 4x4 m² kapalı polyline (DUVAR layer)
    auto poly = std::make_unique<Polyline>(
        std::vector<Vec3>{{0,0,0},{4,0,0},{4,4,0},{0,4,0}}, true);

    // Layer atama (DUVAR)
    LayerManager lm;
    Layer* wallLayer = lm.CreateLayer("DUVAR");
    poly->SetLayer(wallLayer);

    // TEXT entity'ler: bir boyut ("2500"), bir oda ismi ("SALON")
    auto txtDim  = std::make_unique<Text>(Vec3{2,2,0}, "2500");
    auto txtName = std::make_unique<Text>(Vec3{2.1, 2.1, 0}, "SALON");
    txtDim->SetLayer(wallLayer);
    txtName->SetLayer(wallLayer);

    std::vector<Entity*> entities = {poly.get(), txtDim.get(), txtName.get()};

    SpaceDetectionOptions opts;
    opts.wallLayerNames   = {"DUVAR"};
    opts.minArea          = 1.0;
    opts.maxArea          = 500.0;
    opts.drawingUnitsToM2 = 1.0;
    opts.detectNamesFromText = true;

    SpaceManager mgr;
    auto candidates = mgr.DetectSpacesFromEntities(entities, opts);

    REQUIRE(candidates.size() == 1);
    // "2500" boyut yazısı atlanmalı, "SALON" seçilmeli
    REQUIRE(candidates[0].suggestedName == "SALON");
}

// ============================================================
// RemoveOverlappingCandidates
// ============================================================

TEST_CASE("SpaceManager RemoveOverlappingCandidates — removes outer wrapper", "[space][overlap]") {
    using namespace vkt::cad;
    using namespace vkt::geom;

    // Büyük dış çerçeve: 10m×10m = 100 m²
    // İçinde iki küçük oda: 4m×4m = 16 m²
    // RemoveOverlapping → sadece küçük odalar kalmalı

    auto outer = std::make_unique<Polyline>(
        std::vector<Vec3>{{0,0,0},{10,0,0},{10,10,0},{0,10,0}}, true);
    auto room1 = std::make_unique<Polyline>(
        std::vector<Vec3>{{1,1,0},{5,1,0},{5,5,0},{1,5,0}}, true);
    auto room2 = std::make_unique<Polyline>(
        std::vector<Vec3>{{6,1,0},{9,1,0},{9,5,0},{6,5,0}}, true);

    SpaceCandidate cOuter; cOuter.boundary = outer.get(); cOuter.area = 100.0;
    cOuter.center = Vec3{5, 5, 0};
    SpaceCandidate cRoom1; cRoom1.boundary = room1.get(); cRoom1.area = 16.0;
    cRoom1.center = Vec3{3, 3, 0};
    SpaceCandidate cRoom2; cRoom2.boundary = room2.get(); cRoom2.area = 9.0;
    cRoom2.center = Vec3{7.5, 3, 0};

    std::vector<SpaceCandidate> candidates = {cOuter, cRoom1, cRoom2};
    SpaceManager::RemoveOverlappingCandidates(candidates);

    // Dış çerçeve (100 m²) kaldırılmalı; 2 küçük oda kalmalı
    REQUIRE(candidates.size() == 2);
    for (const auto& c : candidates) {
        REQUIRE(c.area < 50.0); // büyük dış çerçeve yok
    }
}

TEST_CASE("SpaceManager RemoveOverlappingCandidates — no false removal for separate rooms", "[space][overlap]") {
    using namespace vkt::cad;
    using namespace vkt::geom;

    // Yan yana iki bağımsız oda — birbirinin içinde değil, ikisi de kalmalı
    auto room1 = std::make_unique<Polyline>(
        std::vector<Vec3>{{0,0,0},{4,0,0},{4,4,0},{0,4,0}}, true);
    auto room2 = std::make_unique<Polyline>(
        std::vector<Vec3>{{5,0,0},{9,0,0},{9,4,0},{5,4,0}}, true);

    SpaceCandidate c1; c1.boundary = room1.get(); c1.area = 16.0; c1.center = {2, 2, 0};
    SpaceCandidate c2; c2.boundary = room2.get(); c2.area = 16.0; c2.center = {7, 2, 0};

    std::vector<SpaceCandidate> candidates = {c1, c2};
    SpaceManager::RemoveOverlappingCandidates(candidates);

    REQUIRE(candidates.size() == 2); // ikisi de kalmalı
}

// ============================================================
// DetectSpacesFromSegments — 2-odalı basit plan
// ============================================================
//
//  (0,4)---(4,4)---(8,4)
//    |       |       |
//  (0,0)---(4,0)---(8,0)
//
// Ortak iç duvar: x=4, y=0..4
// Beklenen: 2 oda (her biri 4x4 = 16 m²)
//

TEST_CASE("SpaceManager DetectSpacesFromSegments — 2-room plan", "[space][segment]") {
    using namespace vkt::cad;
    using namespace vkt::geom;

    // LINE entity'leri oluştur (koordinatlar metre cinsinden, drawingUnitsToM2 = 1.0)
    auto makeLine = [](Vec3 a, Vec3 b) -> std::unique_ptr<Line> {
        return std::make_unique<Line>(a, b);
    };

    std::vector<std::unique_ptr<Line>> lines;
    // Dış çevre
    lines.push_back(makeLine({0,0,0}, {4,0,0}));
    lines.push_back(makeLine({4,0,0}, {8,0,0}));
    lines.push_back(makeLine({8,0,0}, {8,4,0}));
    lines.push_back(makeLine({8,4,0}, {4,4,0}));
    lines.push_back(makeLine({4,4,0}, {0,4,0}));
    lines.push_back(makeLine({0,4,0}, {0,0,0}));
    // Ortak iç duvar
    lines.push_back(makeLine({4,0,0}, {4,4,0}));

    std::vector<Entity*> entities;
    for (auto& l : lines) entities.push_back(l.get());

    SpaceDetectionOptions opts;
    opts.wallLayerNames = {};   // layer filtresi yok
    opts.minArea = 1.0;         // m²
    opts.maxArea = 500.0;
    opts.drawingUnitsToM2 = 1.0; // koordinatlar zaten m cinsinden
    opts.detectNamesFromText = false;

    SpaceManager mgr;
    auto candidates = mgr.DetectSpacesFromSegments(entities, opts, /*snapTolerance=*/0.01);

    REQUIRE(candidates.size() == 2);

    double totalArea = 0.0;
    for (const auto& c : candidates) {
        REQUIRE(c.isValid);
        REQUIRE_THAT(c.area, WithinAbs(16.0, 0.01)); // 4m × 4m
        totalArea += c.area;
    }
    REQUIRE_THAT(totalArea, WithinAbs(32.0, 0.01));
}
