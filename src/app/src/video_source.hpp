#pragma once

#include "pch.hpp"
#include "cl_renderer.hpp"

namespace scpp {

class VideoSource {
protected:
    bool        m_isRunning  = false;
    bool        m_shouldStop = false;
    std::thread m_thread;

    OpenCLRenderer m_renderer;
    VideoSource(const OpenCLDeviceProvider& deviceProviderRef)
        : m_renderer{deviceProviderRef} {}

    VideoSource(VideoSource&&)            = default;
    VideoSource& operator=(VideoSource&&) = default;

public:
    virtual ~VideoSource() {
        if (m_isRunning) {
            Stop();
        }
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
    auto GetRenderer() noexcept -> OpenCLRenderer& {
        return m_renderer;
    }
};

} // namespace scpp