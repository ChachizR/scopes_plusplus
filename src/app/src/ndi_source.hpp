#pragma once

#include "pch.hpp"
#include "video_source.hpp"

namespace scpp {

class NDISource : public VideoSource {
private:
    NDIlib_source_t m_source;

public:
    explicit NDISource(NDIlib_source_t source)
        : m_source{source} {}

    [[nodiscard]]
    ErrorCode Start() override;

    [[nodiscard]]
    virtual std::string_view GetName() const noexcept override;

private:

    void ReceiveLoop();

    [[nodiscard]]
    ErrorCode HandleVideoFrame(const NDIlib_video_frame_v2_t& videoFrame) noexcept;
};
} // namespace scpp