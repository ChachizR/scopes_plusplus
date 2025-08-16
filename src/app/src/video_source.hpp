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

class VideoSource {
protected:
    bool m_isRunning  = false;
    bool m_shouldStop = false;

    std::thread      m_thread;
    OpenCLRenderer   m_renderer;
    VideoSourceStats m_stats;

    VideoSource(const OpenCLDeviceProvider& deviceProviderRef)
        : m_renderer{deviceProviderRef} {}

    VideoSource(VideoSource&&)            = default;
    VideoSource& operator=(VideoSource&&) = default;

public:
    virtual ~VideoSource() {
        if (m_isRunning)
            Stop();
    }

    [[nodiscard]]
    virtual auto GetName() const noexcept -> std::string_view = 0;

    [[nodiscard]]
    constexpr auto IsRunning() const noexcept -> bool { return m_isRunning; }

    [[nodiscard]]
    virtual auto Start() -> ErrorCode = 0;

    void Stop() {
        if (!m_isRunning) {
            return;
        }
        m_shouldStop = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_isRunning = false;
    }

    [[nodiscard]]
    auto GetRenderer() noexcept -> OpenCLRenderer& { return m_renderer; }

    [[nodiscard]]
    auto GetStats() const noexcept -> const VideoSourceStats& { return m_stats; }
};

} // namespace scpp