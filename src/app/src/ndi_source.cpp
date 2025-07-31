#include "pch.hpp"
#include "ndi_source.hpp"

namespace scpp {

ErrorCode NDISource::Start() {
    if (m_isRunning) {
        return ErrorCode::SourceAlreadyRunning;
    }

    m_isRunning  = true;
    m_shouldStop = false;

    m_thread = std::thread([this]() { ReceiveLoop(); });

    return ErrorCode::None;
}

std::string_view NDISource::GetName() const noexcept {
    return std::string_view{m_source.p_ndi_name ? m_source.p_ndi_name : m_source.p_url_address};
}

void NDISource::ReceiveLoop() {
    const NDIlib_recv_create_v3_t recvSettings{
        m_source,
        NDIlib_recv_color_format_RGBX_RGBA,
        NDIlib_recv_bandwidth_highest,
        false,
        "Scopes++ Receiver"};

    const auto recvInstance = NDIlib_recv_create_v3(&recvSettings);

    if (!recvInstance) {
        m_isRunning = false;
        return;
    }

    NDIlib_video_frame_v2_t videoFrameHeader{};

    while (!m_shouldStop) {
        switch (
            NDIlib_recv_capture_v3(recvInstance, &videoFrameHeader, nullptr, nullptr, 100)) {
        case NDIlib_frame_type_none:
        case NDIlib_frame_type_audio:
        case NDIlib_frame_type_metadata:
            break;
        case NDIlib_frame_type_status_change:
            std::println("NDI status change detected.");
            break;
        case NDIlib_frame_type_source_change:
            std::println("NDI source change detected.");
            break;
        case NDIlib_frame_type_error:
            std::println("NDI error detected.");
            m_isRunning  = false;
            m_shouldStop = true;
            break;
        case NDIlib_frame_type_video:
            [[maybe_unused]] auto res = HandleVideoFrame(videoFrameHeader);
            NDIlib_recv_free_video_v2(recvInstance, &videoFrameHeader);
            break;
        }
    }

    NDIlib_recv_destroy(recvInstance);
}

ErrorCode NDISource::HandleVideoFrame([[maybe_unused]] const NDIlib_video_frame_v2_t& videoFrame) noexcept {
    return ErrorCode::None;
}

} // namespace scpp
