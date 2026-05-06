
#include "util/math_utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

namespace scpp::tests {

using namespace Catch::Matchers;

TEST_CASE("Dims2D::operator==", "[Dims2D]") {
    auto dim0 = Dims2D(0u, 0u);

    REQUIRE(dim0 == Dims2D(0u, 0u));

    auto dim1 = Dims2D(1920u, 1080u);

    REQUIRE(dim1 == Dims2D(1920u, 1080u));
    REQUIRE(dim0 != dim1);
}

TEST_CASE("Dims2D::Area", "[Dims2D]") {
    auto dim0 = Dims2D(0u, 0u);
    REQUIRE(dim0.Area() == 0);
    auto dim1 = Dims2D(1920u, 1080u);
    REQUIRE(dim1.Area() == 2073600);
    auto dim2 = Dims2D(65535u, 65535u);
    REQUIRE(dim2.Area() == 4294836225);

    auto dim3 = Dims2D(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max());
    REQUIRE(dim3.Area() == 18446744065119617025ull);
}

TEST_CASE("Dims2D::ToImVec2", "[Dims2D]") {
    auto dim0   = Dims2D(0u, 0u);
    auto imVec0 = dim0.ToImVec2();
    REQUIRE(imVec0.x == 0.0f);
    REQUIRE(imVec0.y == 0.0f);

    auto dim1   = Dims2D(1920u, 1080u);
    auto imVec1 = dim1.ToImVec2();
    REQUIRE(imVec1.x == 1920.0f);
    REQUIRE(imVec1.y == 1080.0f);
}

TEST_CASE("concept Floating", "[math_utils]") {
    CHECK(scpp::Floating<float>);
    CHECK(scpp::Floating<double>);
    CHECK(scpp::Floating<long double>);
    CHECK_FALSE(scpp::Floating<int>);
    CHECK_FALSE(scpp::Floating<uint32_t>);
    CHECK_FALSE(scpp::Floating<char>);
}

TEMPLATE_TEST_CASE("constexpr pow2i 2^n", "[math_utils]", float, double, long double) {
    CHECK(pow2i<TestType>(0) == TestType(1.f));
    CHECK(pow2i<TestType>(1) == TestType(2.f));
    CHECK(pow2i<TestType>(50) == TestType(1125899906842620.f));

    CHECK(pow2i<TestType>(-1) == TestType(0.5f));
    CHECK(pow2i<TestType>(-2) == TestType(0.25f));
    CHECK(pow2i<TestType>(-4) == TestType(0.0625f));
}

TEMPLATE_TEST_CASE("constexpr exp e^n", "[math_utils]", float, double, long double) {
    constexpr auto precisionPercent = 1e-15;

    CHECK(exp_constexpr<TestType>(0) == std::exp(TestType(0)));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(1)), WithinRel(static_cast<double>(std::exp(TestType(1))), precisionPercent));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(2)), WithinRel(static_cast<double>(std::exp(TestType(2))), precisionPercent));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(4)), WithinRel(static_cast<double>(std::exp(TestType(4))), precisionPercent));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(-1)), WithinRel(static_cast<double>(std::exp(TestType(-1))), precisionPercent));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(-2)), WithinRel(static_cast<double>(std::exp(TestType(-2))), precisionPercent));
    CHECK_THAT(static_cast<double>(exp_constexpr<TestType>(-4)), WithinRel(static_cast<double>(std::exp(TestType(-4))), precisionPercent));
}

TEMPLATE_TEST_CASE("mapRange basic mapping and edge behaviour",
                   "[math_utils][mapRange]",
                   float, double, long double) {
    using T                 = TestType;
    constexpr double relTol = 1e-12;

    // Identity mapping: in and out range the same
    CHECK_THAT(static_cast<double>(mapRange<T>(T(5), T(0), T(10), T(0), T(10))), WithinRel(5.0, relTol));

    // Simple scaling: [0, 10] -> [0, 1]
    CHECK_THAT(static_cast<double>(mapRange<T>(T(5), T(0), T(10), T(0), T(1))), WithinRel(0.5, relTol));

    // Lower bound input maps to lower bound output
    CHECK_THAT(static_cast<double>(mapRange<T>(T(0), T(0), T(10), T(-1), T(1))), WithinRel(-1.0, relTol));

    // Upper bound input maps to upper bound output
    CHECK_THAT(static_cast<double>(mapRange<T>(T(10), T(0), T(10), T(-1), T(1))), WithinRel(1.0, relTol));

    // Value below input range -> extrapolation below output range
    CHECK_THAT(static_cast<double>(mapRange<T>(T(-5), T(0), T(10), T(0), T(1))), WithinRel(-0.5, relTol));

    // Value above input range -> extrapolation above output range
    CHECK_THAT(static_cast<double>(mapRange<T>(T(15), T(0), T(10), T(0), T(1))), WithinRel(1.5, relTol));

    // Reversed in-range: [10, 0] -> [0, 1]
    // At in=10 we expect out=0, at in=0 we expect out=1
    CHECK_THAT(static_cast<double>(mapRange<T>(T(10), T(10), T(0), T(0), T(1))), WithinRel(0.0, relTol));
    CHECK_THAT(static_cast<double>(mapRange<T>(T(0), T(10), T(0), T(0), T(1))), WithinRel(1.0, relTol));
}

TEMPLATE_TEST_CASE("mapRangeClampOut clamps output to given range",
                   "[math_utils][mapRangeClampOut]",
                   float, double, long double) {
    using T                 = TestType;
    constexpr double relTol = 1e-12;

    // Input inside range -> mapped value inside clamp range, no clamp
    {
        T value  = T(5);
        T mapped = mapRangeClampOut(value, T(0), T(10), T(0), T(1), T(0), T(1));
        CHECK_THAT(static_cast<double>(mapped), WithinRel(0.5, relTol));
    }

    // Mapped value below clamp -> clamped to outClampMin
    {
        T value  = T(-5); // maps to -0.5 on [0, 10] -> [0, 1]
        T mapped = mapRangeClampOut(value, T(0), T(10), T(0), T(1), T(0), T(1));
        CHECK_THAT(static_cast<double>(mapped), WithinRel(0.0, relTol));
    }

    // Mapped value above clamp -> clamped to outClampMax
    {
        T value  = T(15); // maps to 1.5 on [0, 10] -> [0, 1]
        T mapped = mapRangeClampOut(value, T(0), T(10), T(0), T(1), T(0), T(1));
        CHECK_THAT(static_cast<double>(mapped), WithinRel(1.0, relTol));
    }

    // Clamp range narrower than out range
    {
        T value  = T(5); // maps to 0.5 on [0,10]->[0,2]
        T mapped = mapRangeClampOut(value, T(0), T(10), T(0), T(2), T(0.25), T(0.75));
        CHECK_THAT(static_cast<double>(mapped), WithinRel(0.75, relTol)); // 1.0 clamped to 0.75
    }

    // Exactly at clamp boundaries - should pass through
    {
        T valueLow  = T(0);
        T valueHigh = T(10);

        T mappedLow  = mapRangeClampOut(valueLow, T(0), T(10), T(0), T(1), T(0), T(1));
        T mappedHigh = mapRangeClampOut(valueHigh, T(0), T(10), T(0), T(1), T(0), T(1));

        CHECK_THAT(static_cast<double>(mappedLow), WithinRel(0.0, relTol));
        CHECK_THAT(static_cast<double>(mappedHigh), WithinRel(1.0, relTol));
    }
}

TEMPLATE_TEST_CASE("lerp basic interpolation and extrapolation",
                   "[math_utils][lerp]",
                   float, double, long double) {
    using T                 = TestType;
    constexpr double relTol = 1e-12;

    // t = 0 -> returns a
    CHECK_THAT(static_cast<double>(lerp<T>(T(0), T(10), T(0))), WithinRel(0.0, relTol));

    // t = 1 -> returns b
    CHECK_THAT(static_cast<double>(lerp<T>(T(0), T(10), T(1))), WithinRel(10.0, relTol));

    // t = 0.5 -> midpoint
    CHECK_THAT(static_cast<double>(lerp<T>(T(0), T(10), T(0.5))), WithinRel(5.0, relTol));

    // a > b, still interpolates correctly
    CHECK_THAT(static_cast<double>(lerp<T>(T(10), T(0), T(0.25))), WithinRel(7.5, relTol));

    // Extrapolation: t < 0
    CHECK_THAT(static_cast<double>(lerp<T>(T(0), T(10), T(-1))), WithinRel(-10.0, relTol));

    // Extrapolation: t > 1
    CHECK_THAT(static_cast<double>(lerp<T>(T(0), T(10), T(2))), WithinRel(20.0, relTol));

    // Degenerate case: a == b, result should always be a
    CHECK_THAT(static_cast<double>(lerp<T>(T(5), T(5), T(0.0))), WithinRel(5.0, relTol));
    CHECK_THAT(static_cast<double>(lerp<T>(T(5), T(5), T(0.5))), WithinRel(5.0, relTol));
    CHECK_THAT(static_cast<double>(lerp<T>(T(5), T(5), T(10.0))), WithinRel(5.0, relTol));
}

} // namespace scpp::tests
