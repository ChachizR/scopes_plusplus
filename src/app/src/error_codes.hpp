#pragma once

#include "pch.hpp"

namespace scpp {
enum class ErrorCode {
    None = 0,
    FSNotFound,
    FSReadFailed,
    SourceNotFound,
    SourceAlreadyRunning,
    KernelSourceNotFound,
    KernelCreateProgramFailed,
    KernelProgramBuildFailed,
    KernelCreateKernelFailed,
};

inline constexpr std::string_view ErrorCodeToString(ErrorCode code) {
    switch (code) {
    case ErrorCode::None:
        return "No error";
    case ErrorCode::FSNotFound:
        return "File system not found";
    case ErrorCode::FSReadFailed:
        return "File system read failed";
    case ErrorCode::SourceNotFound:
        return "Source not found";
    case ErrorCode::SourceAlreadyRunning:
        return "Source already running";
    case ErrorCode::KernelSourceNotFound:
        return "Kernel source not found";
    case ErrorCode::KernelCreateProgramFailed:
        return "Kernel create program failed";
    case ErrorCode::KernelProgramBuildFailed:
        return "Kernel program build failed";
    case ErrorCode::KernelCreateKernelFailed:
        return "Kernel create kernel failed";
    }
    return "Unknown error code";
}

} // namespace scpp


template<>
struct std::formatter<scpp::ErrorCode, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template<typename FormatContext>
    auto format(scpp::ErrorCode code, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", scpp::ErrorCodeToString(code));
    }
};