#pragma once

#include "pch.hpp"

namespace scpp {
struct Dims2D {
    uint32_t width;
    uint32_t height;

    constexpr Dims2D(uint32_t w, uint32_t h)
        : width{w}
        , height{h} {}

    constexpr bool operator==(const Dims2D& other) const noexcept {
        return width == other.width && height == other.height;
    }

    constexpr uint64_t Area() const noexcept {
        return static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
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

namespace scpp{
#pragma pack(push, 1)
struct CLRect2D {
    cl_int x;
    cl_int y;
    cl_uint width;
    cl_uint height;
};
#pragma pack(pop)

}