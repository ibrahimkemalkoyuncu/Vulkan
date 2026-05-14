/**
 * @file test_geometry.cpp
 * @brief Geometri birim testleri (Vec3, Mat4, BoundingBox)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "geom/Math.hpp"
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace vkt::geom;

TEST_CASE("Vec3 - Basic Operations", "[geometry]") {
    SECTION("Default constructor initializes to zero") {
        Vec3 v;
        REQUIRE_THAT(v.x, Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(v.y, Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(v.z, Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("Addition") {
        Vec3 a(1.0, 2.0, 3.0);
        Vec3 b(4.0, 5.0, 6.0);
        Vec3 c = a + b;
        REQUIRE_THAT(c.x, Catch::Matchers::WithinAbs(5.0, 1e-12));
        REQUIRE_THAT(c.y, Catch::Matchers::WithinAbs(7.0, 1e-12));
        REQUIRE_THAT(c.z, Catch::Matchers::WithinAbs(9.0, 1e-12));
    }

    SECTION("Subtraction") {
        Vec3 a(5.0, 7.0, 9.0);
        Vec3 b(1.0, 2.0, 3.0);
        Vec3 c = a - b;
        REQUIRE_THAT(c.x, Catch::Matchers::WithinAbs(4.0, 1e-12));
    }

    SECTION("Scalar multiplication") {
        Vec3 v(2.0, 3.0, 4.0);
        Vec3 r = v * 2.0;
        REQUIRE_THAT(r.x, Catch::Matchers::WithinAbs(4.0, 1e-12));
        REQUIRE_THAT(r.y, Catch::Matchers::WithinAbs(6.0, 1e-12));
    }

    SECTION("Length") {
        Vec3 v(3.0, 4.0, 0.0);
        REQUIRE_THAT(v.Length(), Catch::Matchers::WithinAbs(5.0, 1e-9));
    }

    SECTION("Normalize") {
        Vec3 v(0.0, 3.0, 4.0);
        Vec3 n = v.Normalize();
        REQUIRE_THAT(n.Length(), Catch::Matchers::WithinAbs(1.0, 1e-9));
    }

    SECTION("Zero vector normalize") {
        Vec3 v(0.0, 0.0, 0.0);
        Vec3 n = v.Normalize();
        REQUIRE_THAT(n.Length(), Catch::Matchers::WithinAbs(0.0, 1e-9));
    }
}

TEST_CASE("Vec3 - Dot and Cross Product", "[geometry]") {
    SECTION("Dot product of orthogonal vectors is zero") {
        Vec3 a(1.0, 0.0, 0.0);
        Vec3 b(0.0, 1.0, 0.0);
        REQUIRE_THAT(Vec3::Dot(a, b), Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("Dot product of parallel vectors") {
        Vec3 a(2.0, 0.0, 0.0);
        Vec3 b(3.0, 0.0, 0.0);
        REQUIRE_THAT(Vec3::Dot(a, b), Catch::Matchers::WithinAbs(6.0, 1e-12));
    }

    SECTION("Cross product of X and Y gives Z") {
        Vec3 x(1.0, 0.0, 0.0);
        Vec3 y(0.0, 1.0, 0.0);
        Vec3 z = Vec3::Cross(x, y);
        REQUIRE_THAT(z.z, Catch::Matchers::WithinAbs(1.0, 1e-12));
    }
}

TEST_CASE("Vec3 - Distance", "[geometry]") {
    Vec3 a(0.0, 0.0, 0.0);
    Vec3 b(3.0, 4.0, 0.0);
    REQUIRE_THAT(Distance(a, b), Catch::Matchers::WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(a.DistanceTo(b), Catch::Matchers::WithinAbs(5.0, 1e-9));
}

TEST_CASE("Mat4 - Identity", "[geometry]") {
    Mat4 m;
    // Diagonal should be 1
    REQUIRE_THAT(m.data[0], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(m.data[5], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(m.data[10], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(m.data[15], Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    // Off-diagonal should be 0
    REQUIRE_THAT(m.data[1], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Mat4 - Translate", "[geometry]") {
    Mat4 t = Mat4::Translate(Vec3(10.0, 20.0, 30.0));
    Vec3 origin(0.0, 0.0, 0.0);
    Vec3 result = t.Transform(origin);
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(10.0, 0.01));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(20.0, 0.01));
    REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(30.0, 0.01));
}

TEST_CASE("Mat4 - Multiplication with Identity", "[geometry]") {
    Mat4 identity;
    Mat4 t = Mat4::Translate(Vec3(5.0, 5.0, 5.0));
    Mat4 result = identity * t;

    Vec3 p(1.0, 2.0, 3.0);
    Vec3 r1 = t.Transform(p);
    Vec3 r2 = result.Transform(p);

    REQUIRE_THAT(r1.x, Catch::Matchers::WithinAbs(r2.x, 0.01));
    REQUIRE_THAT(r1.y, Catch::Matchers::WithinAbs(r2.y, 0.01));
}

TEST_CASE("Mat4 - Inverse", "[geometry]") {
    Mat4 t = Mat4::Translate(Vec3(10.0, 20.0, 30.0));
    Mat4 inv = t.Inverse();
    Mat4 identity = t * inv;

    // Should be close to identity
    REQUIRE_THAT(identity.data[0], Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(identity.data[5], Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(identity.data[12], Catch::Matchers::WithinAbs(0.0f, 0.01f));
}

// ──────────────────────────────────────────────────────────────────────────────
// Text rotation math (DWG/DXF x_axis_dir → rotation degrees)
// ──────────────────────────────────────────────────────────────────────────────

static constexpr double kPi = 3.14159265358979323846;

static double XAxisDirToRotDeg(double dx, double dy) {
    return std::atan2(dy, dx) * 180.0 / kPi;
}

TEST_CASE("Text rotation - x_axis_dir to degrees", "[text][mtext]") {
    constexpr double EPS = 1e-6;

    SECTION("No rotation: x_axis_dir = (1, 0)") {
        REQUIRE_THAT(XAxisDirToRotDeg(1.0, 0.0), Catch::Matchers::WithinAbs(0.0, EPS));
    }

    SECTION("90 degree rotation: x_axis_dir = (0, 1)") {
        REQUIRE_THAT(XAxisDirToRotDeg(0.0, 1.0), Catch::Matchers::WithinAbs(90.0, EPS));
    }

    SECTION("45 degree rotation: x_axis_dir = (cos45, sin45)") {
        double c = std::cos(kPi / 4.0);
        double s = std::sin(kPi / 4.0);
        REQUIRE_THAT(XAxisDirToRotDeg(c, s), Catch::Matchers::WithinAbs(45.0, EPS));
    }

    SECTION("180 degree rotation: x_axis_dir = (-1, 0)") {
        double deg = XAxisDirToRotDeg(-1.0, 0.0);
        // atan2(0, -1) = ±180° — normalise to [0,360)
        if (deg < 0) deg += 360.0;
        REQUIRE_THAT(deg, Catch::Matchers::WithinAbs(180.0, EPS));
    }

    SECTION("270 degree rotation: x_axis_dir = (0, -1)") {
        double deg = XAxisDirToRotDeg(0.0, -1.0);
        if (deg < 0) deg += 360.0;
        REQUIRE_THAT(deg, Catch::Matchers::WithinAbs(270.0, EPS));
    }

    SECTION("Negative angle normalises to positive") {
        // atan2 returns [-π, π]; negative values shift to [180, 360)
        double deg = XAxisDirToRotDeg(1.0, -1.0); // -45°
        if (deg < 0) deg += 360.0;
        REQUIRE_THAT(deg, Catch::Matchers::WithinAbs(315.0, EPS));
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// MTEXT word-wrap: greedy line-break algorithm
// ──────────────────────────────────────────────────────────────────────────────

// Pure-logic reimplementation of the greedy word-wrap used in SnapOverlay.
// Uses character count as proxy for pixel width (monospace assumption) so the
// test is deterministic without a QFontMetrics.
static std::vector<std::string> WordWrap(const std::string& text, int maxChars) {
    std::vector<std::string> lines;
    if (maxChars <= 0) { lines.push_back(text); return lines; }

    std::istringstream iss(text);
    std::string word, cur;
    while (iss >> word) {
        std::string test = cur.empty() ? word : cur + ' ' + word;
        if ((int)test.size() <= maxChars) {
            cur = test;
        } else {
            if (!cur.empty()) lines.push_back(cur);
            cur = word;
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

TEST_CASE("MTEXT word-wrap - greedy algorithm", "[text][mtext]") {
    SECTION("Short text fits in one line") {
        auto lines = WordWrap("Hello World", 20);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0] == "Hello World");
    }

    SECTION("Text wraps at maxChars boundary") {
        // "Hello World" = 11 chars; maxChars=5 → two lines
        auto lines = WordWrap("Hello World", 5);
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0] == "Hello");
        REQUIRE(lines[1] == "World");
    }

    SECTION("Multi-word wrap") {
        auto lines = WordWrap("one two three four five", 10);
        // "one two" = 7, "three four" = 10 (fits exactly), "five" = 4
        REQUIRE(lines.size() == 3);
        REQUIRE(lines[0] == "one two");
        REQUIRE(lines[1] == "three four");
        REQUIRE(lines[2] == "five");
    }

    SECTION("Single long word is not split") {
        auto lines = WordWrap("Supercalifragilistic", 5);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0] == "Supercalifragilistic");
    }

    SECTION("Empty string produces no lines") {
        auto lines = WordWrap("", 20);
        REQUIRE(lines.empty());
    }

    SECTION("maxChars=0 passes text through unchanged") {
        auto lines = WordWrap("DN50 Su Temini Hatti", 0);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0] == "DN50 Su Temini Hatti");
    }

    SECTION("Exact boundary: word fills maxChars exactly") {
        auto lines = WordWrap("Hello World", 11);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0] == "Hello World");
    }

    SECTION("Realistic MTEXT label — DN pipe annotation") {
        auto lines = WordWrap("DN50 Su Temini Hatti Kat-1", 12);
        // "DN50 Su" = 7, "Temini" = 6, "Hatti" = 5, "Kat-1" = 5
        REQUIRE(lines.size() >= 2);
        REQUIRE(lines[0] == "DN50 Su");
    }
}
