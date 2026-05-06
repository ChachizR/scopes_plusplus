#pragma once

#include "pch.hpp"
#include "cl_renderer.hpp"

namespace scpp {

struct VideoSourceStats {
    Dims2D       sourceDims{0u, 0u};
    float        sourceFPS{0.f};
    SourceFormat sourceFormat{SourceFormat::unknown};
    float        maxRenderFPS{0.f};
    float        renderDurationMS{0.f};
    float        avgMaxRenderFPS{0.f};
    float        avgRenderDurationMS{0.f};

    void SetRenderTimings(Clock::duration renderDuration) noexcept {
        renderDurationMS = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(renderDuration).count()) / 1e3f;

        avgRenderDurationMS = (avgRenderDurationMS * 0.99f) + (renderDurationMS * 0.01f);

        if (renderDurationMS > 0.f)
            maxRenderFPS = 1e3f / renderDurationMS;
        else
            maxRenderFPS = 0.f;

        avgMaxRenderFPS = (avgMaxRenderFPS * 0.99f) + (maxRenderFPS * 0.01f);
    }
};

struct SourceFrameView {
    std::span<const uint8_t> data;
    Dims2D                   dims{0u, 0u};
    SourceFormat             format{SourceFormat::unknown};
    uint32_t                 lineStrideBytes{0u};
    uint64_t                 sequence{0u};
};

class VideoSource {
protected:
    bool m_isRunning  = false;
    bool m_shouldStop = false;

    std::thread      m_thread;
    std::unique_ptr<OpenCLRenderer> m_renderer;
    VideoSourceStats m_stats;
    RenderSettings   m_renderSettings;

    VideoSource(const OpenCLDeviceProvider& deviceProviderRef)
        : m_renderer{std::make_unique<OpenCLRenderer>(deviceProviderRef)} {}

    VideoSource() = default;

    VideoSource(VideoSource&&)            = delete;
    VideoSource& operator=(VideoSource&&) = delete;

public:
    virtual ~VideoSource() {
        Stop();
    }

    [[nodiscard]]
    virtual auto GetName() const noexcept -> std::string_view = 0;

    [[nodiscard]]
    constexpr auto IsRunning() const noexcept -> bool { return m_isRunning; }

    [[nodiscard]]
    virtual auto Start() -> ErrorCode = 0;

    virtual void UpdateOnMainThread() noexcept {}

    [[nodiscard]]
    virtual auto GetSourcePreviewFrame() const noexcept -> std::optional<SourceFrameView> {
        return std::nullopt;
    }

    void Stop() {
        m_shouldStop = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_isRunning = false;
    }

    [[nodiscard]]
    auto HasOpenCLRenderer() const noexcept -> bool { return m_renderer != nullptr; }

    [[nodiscard]]
    auto GetRenderer() noexcept -> OpenCLRenderer& { return *m_renderer; }

    [[nodiscard]]
    auto GetStats() const noexcept -> const VideoSourceStats& { return m_stats; }

    [[nodiscard]]
    auto GetRenderSettings() noexcept -> RenderSettings& { return m_renderSettings; }
};

} // namespace scpp
