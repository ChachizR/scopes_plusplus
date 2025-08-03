#pragma once

#include "pch.hpp"

#include "error_codes.hpp"
#include "bitmap.hpp"

namespace scpp {

[[nodiscard]]
inline std::expected<std::vector<uint8_t>, ErrorCode> LoadFile(
    const std::filesystem::path& path) {

    if (!std::filesystem::exists(path)) {
        return std::unexpected(ErrorCode::FSNotFound);
    }

    if (!std::filesystem::is_regular_file(path)) {
        return std::unexpected(ErrorCode::FSNotFound);
    }

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return std::unexpected(ErrorCode::FSReadFailed);
    }

    auto size = std::filesystem::file_size(path);

    std::vector<uint8_t> data;
    data.resize(size);

    if (size > 0) {

        file.read(reinterpret_cast<char*>(data.data()), size);
        if (!file) {
            return std::unexpected(ErrorCode::FSReadFailed);
        }
    }

    return data;
}

[[nodiscard]]
inline std::expected<BitmapData, ErrorCode> LoadBitmapFromBinary(
    const std::filesystem::path& path,
    uint32_t width, uint32_t height, BitmapFormat format) {

    if (!std::filesystem::exists(path)) {
        return std::unexpected(ErrorCode::FSNotFound);
    }

    if (!std::filesystem::is_regular_file(path)) {
        return std::unexpected(ErrorCode::FSNotFound);
    }

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return std::unexpected(ErrorCode::FSReadFailed);
    }

    const auto size = std::filesystem::file_size(path);

    if (size == 0) {
        return std::unexpected(ErrorCode::FSReadFailed);
    }

    BitmapData bitmapData;

    bitmapData.width  = width;
    bitmapData.height = height;
    bitmapData.format = format;

    bitmapData.data = std::make_unique<uint8_t[]>(size);

    file.read(reinterpret_cast<char*>(bitmapData.data.get()), size);

    if (!file) {
        return std::unexpected(ErrorCode::FSReadFailed);
    }

    return bitmapData;
}

} // namespace scpp
