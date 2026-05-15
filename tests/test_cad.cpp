/**
 * @file test_cad.cpp
 * @brief CAD entity birim testleri — Line, Circle, Arc, Polyline
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/Ellipse.hpp"
#include "cad/Spline.hpp"
#include "cad/Polyline.hpp"
#include "cad/LayerManager.hpp"
#include <cmath>

using namespace vkt::cad;
using namespace vkt::geom;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ============================================================
// LINE
// ============================================================

TEST_CASE("Line basic properties", "[cad][line]") {
    Line line({0,0,0}, {3,4,0});

    REQUIRE_THAT(line.GetLength(), WithinAbs(5.0, 1e-9));
    REQUIRE(line.GetType() == EntityType::Line);

    Vec3 mid = line.GetMidPoint();
    REQUIRE_THAT(mid.x, WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(mid.y, WithinAbs(2.0, 1e-9));
}

TEST_CASE("Line Move", "[cad][line]") {
    Line line({1,1,0}, {3,3,0});
    line.Move({2,0,0});
    REQUIRE_THAT(line.GetStart().x, WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(line.GetEnd().x,   WithinAbs(5.0, 1e-9));
}

TEST_CASE("Line BoundingBox", "[cad][line]") {
    Line line({1,2,0}, {5,4,0});
    auto bb = line.GetBounds();
    REQUIRE_THAT(bb.min.x, WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(bb.max.x, WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(bb.min.y, WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(bb.max.y, WithinAbs(4.0, 1e-9));
}

TEST_CASE("Line Clone", "[cad][line]") {
    Line line({0,0,0}, {1,1,0});
    auto clone = line.Clone();
    REQUIRE(clone->GetType() == EntityType::Line);
    Line* clonedLine = static_cast<Line*>(clone.get());
    REQUIRE_THAT(clonedLine->GetLength(), WithinAbs(line.GetLength(), 1e-9));
}

// ============================================================
// CIRCLE
// ============================================================

TEST_CASE("Circle basic properties", "[cad][circle]") {
    Circle circle({0,0,0}, 5.0);

    REQUIRE_THAT(circle.GetRadius(),      WithinAbs(5.0,               1e-9));
    REQUIRE_THAT(circle.GetArea(),        WithinRel(PI * 25.0,         1e-6));
    REQUIRE_THAT(circle.GetCircumference(), WithinRel(2.0 * PI * 5.0, 1e-6));
    REQUIRE(circle.GetType() == EntityType::Circle);
}

TEST_CASE("Circle Scale", "[cad][circle]") {
    Circle circle({0,0,0}, 3.0);
    circle.Scale({2.0, 2.0, 1.0});
    REQUIRE_THAT(circle.GetRadius(), WithinAbs(6.0, 1e-9));
}

TEST_CASE("Circle contains point", "[cad][circle]") {
    Circle circle({0,0,0}, 5.0);
    REQUIRE(circle.ContainsPoint({3,4,0}));    // on boundary
    REQUIRE(circle.ContainsPoint({0,0,0}));    // center
    REQUIRE_FALSE(circle.ContainsPoint({5,5,0})); // outside
}

// ============================================================
// ARC
// ============================================================

TEST_CASE("Arc basic properties", "[cad][arc]") {
    Arc arc({0,0,0}, 10.0, 0.0, 90.0);

    REQUIRE_THAT(arc.GetRadius(), WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(arc.GetLength(), WithinRel(PI * 10.0 / 2.0, 1e-6)); // quarter circle
    REQUIRE(arc.GetType() == EntityType::Arc);
}

TEST_CASE("Arc start/end points", "[cad][arc]") {
    Arc arc({0,0,0}, 5.0, 0.0, 90.0);

    Vec3 start = arc.GetStartPoint();
    Vec3 end   = arc.GetEndPoint();

    REQUIRE_THAT(start.x, WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(start.y, WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(end.x,   WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(end.y,   WithinAbs(5.0, 1e-6));
}

TEST_CASE("Arc CreateFrom3Points circumcircle", "[cad][arc]") {
    // Three points on a circle of radius 5 centered at (0,0)
    Vec3 p1{5, 0, 0};
    Vec3 p2{0, 5, 0};
    Vec3 p3{-5, 0, 0};

    auto arc = Arc::CreateFrom3Points(p1, p2, p3);
    REQUIRE(arc != nullptr);
    REQUIRE_THAT(arc->GetCenter().x, WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(arc->GetCenter().y, WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(arc->GetRadius(),   WithinAbs(5.0, 1e-6));
}

TEST_CASE("Arc CreateFrom3Points collinear degeneracy", "[cad][arc]") {
    // Collinear points — should not crash
    auto arc = Arc::CreateFrom3Points({0,0,0}, {1,0,0}, {2,0,0});
    REQUIRE(arc != nullptr); // returns degenerate arc, but no crash
}

// ============================================================
// POLYLINE
// ============================================================

TEST_CASE("Polyline basic vertex management", "[cad][polyline]") {
    Polyline poly;
    poly.AddVertex({0,0,0});
    poly.AddVertex({1,0,0});
    poly.AddVertex({1,1,0});

    REQUIRE(poly.GetVertexCount() == 3);
    REQUIRE(poly.GetType() == EntityType::Polyline);
    REQUIRE_THAT(poly.GetLength(), WithinAbs(2.0, 1e-9));
}

TEST_CASE("Polyline closed area", "[cad][polyline]") {
    // Unit square CCW
    Polyline poly({{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}}, true);
    REQUIRE_THAT(poly.GetArea(), WithinAbs(1.0, 1e-9));
    REQUIRE(poly.IsCounterClockwise());
}

TEST_CASE("Polyline Simplify Douglas-Peucker", "[cad][polyline]") {
    // Straight line with intermediate collinear points
    Polyline poly;
    for (int i = 0; i <= 10; ++i) poly.AddVertex({(double)i, 0, 0});

    REQUIRE(poly.GetVertexCount() == 11);
    poly.Simplify(0.001);
    REQUIRE(poly.GetVertexCount() == 2); // only endpoints remain
}

TEST_CASE("Polyline Simplify preserves significant kinks", "[cad][polyline]") {
    Polyline poly;
    poly.AddVertex({0, 0, 0});
    poly.AddVertex({5, 0, 0});
    poly.AddVertex({5, 5, 0}); // 90-degree turn — must be kept
    poly.AddVertex({10,5, 0});

    poly.Simplify(0.1);
    REQUIRE(poly.GetVertexCount() == 4); // all vertices kept
}

TEST_CASE("Polyline RemoveRedundantVertices removes collinear", "[cad][polyline]") {
    Polyline poly;
    poly.AddVertex({0, 0, 0});
    poly.AddVertex({1, 0, 0}); // collinear — should be removed
    poly.AddVertex({2, 0, 0});
    poly.AddVertex({2, 2, 0}); // turn — kept
    poly.AddVertex({4, 2, 0});

    poly.RemoveRedundantVertices(1e-6);
    REQUIRE(poly.GetVertexCount() == 4); // {0,0}, {2,0}, {2,2}, {4,2}
}

TEST_CASE("Polyline ConvertArcsToLines - no arcs is a no-op", "[cad][polyline]") {
    Polyline poly({{{0,0,0},{1,0,0},{1,1,0}}}, false);
    size_t before = poly.GetVertexCount();
    poly.ConvertArcsToLines(8);
    REQUIRE(poly.GetVertexCount() == before); // unchanged
}

TEST_CASE("Polyline Offset produces correct number of vertices", "[cad][polyline]") {
    // Simple triangle
    Polyline poly({{{0,0,0},{4,0,0},{2,3,0}}}, true);
    auto offset = poly.Offset(0.5);
    REQUIRE(offset != nullptr);
    REQUIRE(offset->GetVertexCount() == poly.GetVertexCount());
}

// ============================================================
// ELLIPSE
// ============================================================

TEST_CASE("Ellipse - full ellipse detection", "[cad][ellipse]") {
    static constexpr double kTwoPi = 6.283185307179586;
    Ellipse full({0,0,0}, 5.0, 0.5, 0.0, 0.0, kTwoPi);
    REQUIRE(full.IsFullEllipse());

    Ellipse arc({0,0,0}, 5.0, 0.5, 0.0, 0.0, kTwoPi * 0.5);
    REQUIRE_FALSE(arc.IsFullEllipse());
}

TEST_CASE("Ellipse - GetPointAt circle degenerate", "[cad][ellipse]") {
    // axisRatio=1 → circle; point at t=0 should be (r,0,0)
    Ellipse e({0,0,0}, 3.0, 1.0, 0.0, 0.0, 6.283185307179586);
    auto p = e.GetPointAt(0.0);
    REQUIRE_THAT(p.x, Catch::Matchers::WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(p.y, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Ellipse - GetPointAt with rotation", "[cad][ellipse]") {
    static constexpr double kPi = 3.14159265358979323846;
    // 90-degree rotation: major axis points along Y
    Ellipse e({0,0,0}, 4.0, 0.5, kPi / 2.0, 0.0, 2.0 * kPi);
    auto p = e.GetPointAt(0.0); // t=0 → major axis direction
    REQUIRE_THAT(p.x, Catch::Matchers::WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(p.y, Catch::Matchers::WithinAbs(4.0, 1e-9));
}

TEST_CASE("Ellipse - Tessellate segment count", "[cad][ellipse]") {
    Ellipse e({0,0,0}, 2.0, 0.5, 0.0, 0.0, 6.283185307179586);
    auto pts = e.Tessellate(32);
    REQUIRE(pts.size() == 33); // segments+1
    // First and last point should be close (full ellipse closes)
    REQUIRE_THAT(pts.front().x, Catch::Matchers::WithinAbs(pts.back().x, 1e-9));
    REQUIRE_THAT(pts.front().y, Catch::Matchers::WithinAbs(pts.back().y, 1e-9));
}

TEST_CASE("Ellipse - Move", "[cad][ellipse]") {
    Ellipse e({1,2,0}, 3.0, 0.5, 0.0, 0.0, 6.283185307179586);
    e.Move({10, -5, 0});
    REQUIRE_THAT(e.GetCenter().x, Catch::Matchers::WithinAbs(11.0, 1e-9));
    REQUIRE_THAT(e.GetCenter().y, Catch::Matchers::WithinAbs(-3.0, 1e-9));
}

TEST_CASE("Ellipse - Clone preserves parameters", "[cad][ellipse]") {
    Ellipse e({3,4,0}, 6.0, 0.3, 1.2, 0.5, 5.0);
    auto c = e.Clone();
    const auto* ec = static_cast<const Ellipse*>(c.get());
    REQUIRE(ec->GetType() == EntityType::Ellipse);
    REQUIRE_THAT(ec->GetSemiMajor(),  Catch::Matchers::WithinAbs(6.0, 1e-9));
    REQUIRE_THAT(ec->GetAxisRatio(),  Catch::Matchers::WithinAbs(0.3, 1e-9));
    REQUIRE_THAT(ec->GetRotAngle(),   Catch::Matchers::WithinAbs(1.2, 1e-9));
    REQUIRE_THAT(ec->GetCenter().x,   Catch::Matchers::WithinAbs(3.0, 1e-9));
}

// ============================================================
// SPLINE
// ============================================================

TEST_CASE("Spline - fit points used directly in Tessellate", "[cad][spline]") {
    Spline s;
    s.AddFitPoint({0,0,0});
    s.AddFitPoint({1,2,0});
    s.AddFitPoint({3,1,0});
    REQUIRE(s.HasFitPoints());
    auto pts = s.Tessellate();
    REQUIRE(pts.size() == 3);
    REQUIRE_THAT(pts[0].x, Catch::Matchers::WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(pts[1].x, Catch::Matchers::WithinAbs(1.0, 1e-9));
}

TEST_CASE("Spline - De Boor: cubic B-spline uniform knots", "[cad][spline]") {
    // 4 control points, degree 3, uniform clamped knot vector [0,0,0,0,1,1,1,1]
    Spline s;
    s.SetDegree(3);
    std::vector<Vec3> ctrl = {{0,0,0},{1,2,0},{2,2,0},{3,0,0}};
    s.SetCtrlPoints(ctrl);
    s.SetKnots({0,0,0,0,1,1,1,1});
    REQUIRE(s.HasCtrlPoints());
    REQUIRE_FALSE(s.HasFitPoints());
    auto pts = s.Tessellate(16);
    REQUIRE(pts.size() == 17);
    // First and last should match first and last control points (clamped)
    REQUIRE_THAT(pts.front().x, Catch::Matchers::WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(pts.back().x,  Catch::Matchers::WithinAbs(3.0, 1e-6));
}

TEST_CASE("Spline - Move transforms fit and ctrl points", "[cad][spline]") {
    Spline s;
    s.AddFitPoint({1,1,0});
    s.SetCtrlPoints({{2,2,0}});
    s.Move({5, -3, 0});
    REQUIRE_THAT(s.GetFitPoints()[0].x,  Catch::Matchers::WithinAbs(6.0, 1e-9));
    REQUIRE_THAT(s.GetFitPoints()[0].y,  Catch::Matchers::WithinAbs(-2.0, 1e-9));
    REQUIRE_THAT(s.GetCtrlPoints()[0].x, Catch::Matchers::WithinAbs(7.0, 1e-9));
}

TEST_CASE("Spline - Clone preserves data", "[cad][spline]") {
    Spline s;
    s.SetDegree(2);
    s.SetClosed(true);
    s.AddFitPoint({1,0,0});
    s.AddFitPoint({2,3,0});
    auto c = s.Clone();
    const auto* sc = static_cast<const Spline*>(c.get());
    REQUIRE(sc->GetType() == EntityType::Spline);
    REQUIRE(sc->GetDegree() == 2);
    REQUIRE(sc->IsClosed());
    REQUIRE(sc->GetFitPoints().size() == 2);
}

TEST_CASE("Spline - empty yields no tessellate output", "[cad][spline]") {
    Spline s;
    auto pts = s.Tessellate();
    REQUIRE(pts.empty());
}

// ============================================================
// LAYER MANAGER
// ============================================================

TEST_CASE("LayerManager default layer 0", "[cad][layer]") {
    LayerManager lm;
    REQUIRE(lm.HasLayer("0"));
    REQUIRE(lm.GetActiveLayer() != nullptr);
    REQUIRE(lm.GetActiveLayerName() == "0");
}

TEST_CASE("LayerManager create and delete layer", "[cad][layer]") {
    LayerManager lm;
    Layer* l = lm.CreateLayer("BORU", Color::White());
    REQUIRE(l != nullptr);
    REQUIRE(lm.HasLayer("BORU"));

    bool deleted = lm.DeleteLayer("BORU", 0);
    REQUIRE(deleted);
    REQUIRE_FALSE(lm.HasLayer("BORU"));
}

TEST_CASE("LayerManager delete layer with entities blocked", "[cad][layer]") {
    LayerManager lm;
    lm.CreateLayer("TEST");

    // entityCount=3 → deletion must be refused
    bool deleted = lm.DeleteLayer("TEST", 3);
    REQUIRE_FALSE(deleted);
    REQUIRE(lm.HasLayer("TEST")); // still exists
}

TEST_CASE("LayerManager cannot delete layer 0", "[cad][layer]") {
    LayerManager lm;
    REQUIRE_FALSE(lm.DeleteLayer("0", 0));
}

TEST_CASE("LayerManager cannot delete active layer", "[cad][layer]") {
    LayerManager lm;
    lm.CreateLayer("ACTIVE");
    lm.SetActiveLayer("ACTIVE");
    REQUIRE_FALSE(lm.DeleteLayer("ACTIVE", 0));
}
