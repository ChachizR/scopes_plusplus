#pragma once

#include "pch.hpp"
#include "video_source.hpp"

namespace scpp {

class NDISource : public VideoSource {
private:
    NDIlib_source_t m_source;

public:
    explicit NDISource(const OpenCLDeviceProvider& deviceProviderRef, NDIlib_source_t source) noexcept
        : VideoSource{deviceProviderRef}
        , m_source{source} {}

    auto Start() -> ErrorCode override;

    [[nodiscard]]
    auto GetName() const noexcept -> std::string_view override;

private:
    void ReceiveLoop();

    [[nodiscard]]
    auto HandleVideoFrame(const NDIlib_video_frame_v2_t& videoFrame) noexcept -> ErrorCode;
};
} // namespace scpp