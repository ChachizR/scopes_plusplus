#pragma once

#include "pch.hpp"
#include "video_source.hpp"

namespace scpp {

class VideoFileSource : public VideoSource {
private:
    struct PendingFrame {
        std::vector<uint8_t> data;
        Dims2D               dims{0u, 0u};
        uint32_t             lineStrideBytes{0u};
        bool                 ready{false};
        uint64_t             sequence{0u};
    };

    std::filesystem::path m_path;
    std::string           m_name;
    std::mutex            m_frameMutex;
    PendingFrame          m_latestDecodedFrame;
    PendingFrame          m_stagedFrame;
    uint64_t              m_decodedSequence{0u};
    uint64_t              m_lastRenderedSequence{0u};
    float                 m_nominalSourceFPS{0.0f};

public:
    VideoFileSource(const OpenCLDeviceProvider& deviceProviderRef, std::filesystem::path path) noexcept;
    explicit VideoFileSource(std::filesystem::path path) noexcept;

    auto Start() -> ErrorCode override;

    void UpdateOnMainThread() noexcept override;

    [[nodiscard]]
    auto GetSourcePreviewFrame() const noexcept -> std::optional<SourceFrameView> override;

    [[nodiscard]]
    auto GetName() const noexcept -> std::string_view override {
        return m_name;
    }

private:
    void DecodeLoop();
};

} // namespace scpp
