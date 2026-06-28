/**
 * @file test_dxf_roundtrip.cpp
 * @brief Dimension + Leader entity round-trip and geometry tests
 */

#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "cad/Dimension.hpp"
#include "cad/Leader.hpp"
#include "geom/Math.hpp"

using namespace vkt;
using namespace vkt::cad;

// ─────────────────────────────────────────────────────────────────────────────
// Dimension entity
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Dimension - Aligned measured value horizontal", "[roundtrip]") {
    Dimension dim({0,0,0}, {1000,0,0}, {500,200,0}, DimensionType::Aligned);
    REQUIRE(dim.GetMeasuredValue() == Catch::Approx(1000.0).epsilon(1e-6));
}

TEST_CASE("Dimension - Aligned measured value diagonal", "[roundtrip]") {
    Dimension dim({0,0,0}, {300,400,0}, {150,300,0}, DimensionType::Aligned);
    REQUIRE(dim.GetMeasuredValue() == Catch::Approx(500.0).epsilon(1e-6));
}

TEST_CASE("Dimension - Override text returned by GetDisplayText", "[roundtrip]") {
    Dimension dim({0,0,0}, {100,0,0}, {50,10,0});
    REQUIRE(dim.GetDisplayText() != "custom");
    dim.SetOverrideText("custom");
    REQUIRE(dim.GetDisplayText() == "custom");
}

TEST_CASE("Dimension - Serialize Deserialize roundtrip", "[roundtrip]") {
    Dimension orig({100,200,0}, {500,200,0}, {300,350,0}, DimensionType::Aligned);
    orig.SetOverrideText("4.00m");
    orig.SetLayerName("DIMS");

    nlohmann::json j;
    orig.Serialize(j);

    Dimension copy;
    copy.Deserialize(j);

    REQUIRE(copy.GetPoint1().x == Catch::Approx(100.0));
    REQUIRE(copy.GetPoint2().x == Catch::Approx(500.0));
    REQUIRE(copy.GetDimLinePos().y == Catch::Approx(350.0));
    REQUIRE(copy.GetOverrideText() == "4.00m");
    REQUIRE(copy.GetLayerName() == "DIMS");
}

TEST_CASE("Dimension - Clone preserves geometry and style", "[roundtrip]") {
    Dimension orig({0,0,0}, {600,0,0}, {300,150,0}, DimensionType::Linear);
    orig.SetArrowSize(5.0);
    orig.SetTextHeight(3.5);

    auto clone = orig.Clone();
    auto* dc = dynamic_cast<Dimension*>(clone.get());
    REQUIRE(dc != nullptr);
    REQUIRE(dc->GetArrowSize() == Catch::Approx(5.0));
    REQUIRE(dc->GetTextHeight() == Catch::Approx(3.5));
    REQUIRE(dc->GetPoint2().x == Catch::Approx(600.0));
    REQUIRE(dc->GetDimType() == DimensionType::Linear);
}

TEST_CASE("Dimension - Move transform shifts all points", "[roundtrip]") {
    Dimension dim({100,100,0}, {400,100,0}, {250,200,0});
    dim.Move({50, -30, 0});
    REQUIRE(dim.GetPoint1().x == Catch::Approx(150.0));
    REQUIRE(dim.GetPoint1().y == Catch::Approx(70.0));
    REQUIRE(dim.GetDimLinePos().y == Catch::Approx(170.0));
}

TEST_CASE("Dimension - Radius type GetMeasuredValue is center-arc distance", "[roundtrip]") {
    // center=(0,0), arc point=(100,0) -> radius=100
    Dimension dim({0,0,0}, {100,0,0}, {50,0,0}, DimensionType::Radius);
    REQUIRE(dim.GetMeasuredValue() == Catch::Approx(100.0).epsilon(1e-6));
}

// ─────────────────────────────────────────────────────────────────────────────
// Leader entity
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Leader - Vertex storage and retrieval", "[roundtrip]") {
    std::vector<geom::Vec3> verts = {{0,0,0},{100,50,0},{200,100,0}};
    Leader ldr(verts);
    REQUIRE(ldr.GetVertices().size() == 3u);
    REQUIRE(ldr.GetVertices()[0].x == Catch::Approx(0.0));
    REQUIRE(ldr.GetVertices()[2].x == Catch::Approx(200.0));
}

TEST_CASE("Leader - Bounding box encloses all vertices", "[roundtrip]") {
    std::vector<geom::Vec3> verts = {{10,20,0},{90,80,0},{50,50,0}};
    Leader ldr(verts);
    auto bb = ldr.GetBounds();
    REQUIRE(bb.min.x == Catch::Approx(10.0));
    REQUIRE(bb.max.x == Catch::Approx(90.0));
    REQUIRE(bb.min.y == Catch::Approx(20.0));
    REQUIRE(bb.max.y == Catch::Approx(80.0));
}

TEST_CASE("Leader - Serialize Deserialize roundtrip", "[roundtrip]") {
    std::vector<geom::Vec3> verts = {{0,0,0},{100,100,0},{200,80,0}};
    Leader orig(verts, "DN25");
    orig.SetLayerName("ANNO");

    nlohmann::json j;
    orig.Serialize(j);

    Leader copy;
    copy.Deserialize(j);

    REQUIRE(copy.GetVertices().size() == 3u);
    REQUIRE(copy.GetVertices()[1].x == Catch::Approx(100.0));
    REQUIRE(copy.GetText() == "DN25");
    REQUIRE(copy.GetLayerName() == "ANNO");
}

TEST_CASE("Leader - Clone preserves vertices and annotation", "[roundtrip]") {
    std::vector<geom::Vec3> verts = {{0,0,0},{500,300,0}};
    Leader orig(verts, "note");
    auto clone = orig.Clone();
    auto* lc = dynamic_cast<Leader*>(clone.get());
    REQUIRE(lc != nullptr);
    REQUIRE(lc->GetVertices().size() == 2u);
    REQUIRE(lc->GetText() == "note");
}

TEST_CASE("Leader - Move shifts all vertices", "[roundtrip]") {
    std::vector<geom::Vec3> verts = {{100,100,0},{300,200,0}};
    Leader ldr(verts);
    ldr.Move({50, -50, 0});
    REQUIRE(ldr.GetVertices()[0].x == Catch::Approx(150.0));
    REQUIRE(ldr.GetVertices()[0].y == Catch::Approx(50.0));
    REQUIRE(ldr.GetVertices()[1].x == Catch::Approx(350.0));
}

TEST_CASE("Leader - Arrowhead flag default true", "[roundtrip]") {
    Leader ldr;
    REQUIRE(ldr.HasArrowhead() == true);
    ldr.SetArrowhead(false);
    REQUIRE(ldr.HasArrowhead() == false);
}

TEST_CASE("Leader - GetType is EntityType Leader", "[roundtrip]") {
    Leader ldr;
    REQUIRE(ldr.GetType() == EntityType::Leader);
}
