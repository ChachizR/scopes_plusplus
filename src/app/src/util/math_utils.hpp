#pragma once

#include <format>
#include <algorithm>

#include <CL/opencl.hpp>
#include <imgui.h>

namespace scpp {
struct Dims2D {
    uint32_t width;
    uint32_t height;

    constexpr Dims2D(int32_t w, int32_t h) = delete;

    constexpr Dims2D(uint32_t w, uint32_t h)
        : width{w}
        , height{h} {}

    constexpr bool operator==(const Dims2D& other) const noexcept {
        return width == other.width && height == other.height;
    }

    constexpr auto Area() const noexcept -> uint64_t {
        return static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    }

    constexpr auto ToImVec2() const noexcept -> ImVec2 {
        return ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
};

} // namespace scpp

template<>
struct std::formatter<scpp::Dims2D, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const scpp::Dims2D& dims, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}x{}", dims.width, dims.height);
    }
};

namespace scpp {
#pragma pack(push, 1)
struct CLRect2D {
    cl_int  x;
    cl_int  y;
    cl_uint width;
    cl_uint height;
};
#pragma pack(pop)

template<class T>
concept Floating = std::is_floating_point_v<T>;

template<Floating T>
constexpr T pow2i(long long n) {
    // Compute 2^n using exponentiation by squaring (works for negative n too).
    T                  result = T(1);
    T                  base   = (n >= 0) ? T(2) : T(0.5);
    unsigned long long m      = (n >= 0) ? static_cast<unsigned long long>(n)
                                         : static_cast<unsigned long long>(-n);
    while (m) {
        if (m & 1ULL)
            result *= base;
        base *= base;
        m >>= 1ULL;
    }
    return result;
}

template<Floating T>
constexpr T exp_constexpr(T x) {
    // High-precision ln(2) literal, converted to T.
    const T LN2      = T(0.693147180559945309417232121458176568L);
    const T HALF_LN2 = LN2 * T(0.5);

    // Range reduction: x = n*ln(2) + r, with r in [-ln2/2, ln2/2].
    long long n = static_cast<long long>(x / LN2); // trunc toward 0
    T         r = x - T(n) * LN2;
    if (r > HALF_LN2) {
        ++n;
        r -= LN2;
    }
    if (r < -HALF_LN2) {
        --n;
        r += LN2;
    }

    // e^r via Taylor series sum_{k=0..N} r^k / k!, r is small so this converges fast.
    // N=20 is already ~1e-14 relative error for double on this interval.
    constexpr int N    = 20;
    T             term = T(1);
    T             sum  = T(1);
    for (int k = 1; k <= N; ++k) {
        term *= r / T(k);
        sum += term;
    }

    // Scale back by 2^n.
    return sum * pow2i<T>(n);
}

template<Floating T>
constexpr T mapRange(T value, T inMin, T inMax, T outMin, T outMax) { return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin); }

template<Floating T>
constexpr T mapRangeClampOut(T value, T inMin, T inMax, T outMin, T outMax, T outClampMin, T outClampMax) {
    return std::clamp(mapRange(value, inMin, inMax, outMin, outMax), outClampMin, outClampMax);
}

template<Floating T>
constexpr T lerp(T a, T b, T t) { return a + t * (b - a); }

} // namespace scpp