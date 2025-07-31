#pragma once

#include "pch.hpp"
#include "error_codes.hpp"

namespace scpp {

enum class SourceFormat {
    unknown = 0,
    RGBA_8888,
    BGRA_8888,
    ARGB_8888,
    BGRX_8888,
    BGR_888_InvY,
    RGB_888,
    UYVY_422,
    YUYV_422,
    NV12,
    UYVY10_422,
};

struct SourceFrame {
    std::vector<uint8_t> data;
    size_t               width;
    size_t               height;
    SourceFormat         format;
};

enum class GetFrameResult {
    Success,
    NoFrame,
    Error,
};

class VideoSource {
protected:
    bool        m_isRunning  = false;
    bool        m_shouldStop = false;
    std::thread m_thread;

    VideoSource() {}

    VideoSource(VideoSource&&)                 = default;
    VideoSource& operator=(VideoSource&&)      = default;

public:
    ~VideoSource() {
        if (m_isRunning) {
            Stop();
        }
    }

    [[nodiscard]]
    virtual std::string_view GetName() const noexcept = 0;

    [[nodiscard]]
    constexpr bool IsRunning() const noexcept { return m_isRunning; }

    [[nodiscard]]
    virtual ErrorCode Start() = 0;

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
};

} // namespace scpp