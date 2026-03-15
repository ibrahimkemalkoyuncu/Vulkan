/**
 * @file test_geometry.cpp
 * @brief Geometri birim testleri (Vec3, Mat4, BoundingBox)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "geom/Math.hpp"

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
