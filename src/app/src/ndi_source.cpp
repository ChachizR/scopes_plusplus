#include "pch.hpp"
#include "ndi_source.hpp"

namespace scpp {

auto NDISource::Start() -> ErrorCode {
    if (m_isRunning) {
        return ErrorCode::SourceAlreadyRunning;
    }

    m_isRunning  = true;
    m_shouldStop = false;

    m_thread = std::thread([this]() { ReceiveLoop(); });

    return ErrorCode::None;
}

auto NDISource::GetName() const noexcept -> std::string_view {
    return std::string_view{m_source.p_ndi_name ? m_source.p_ndi_name : m_source.p_url_address};
}

void NDISource::ReceiveLoop() {
    const NDIlib_recv_create_v3_t recvSettings{
        m_source,
        NDIlib_recv_color_format_fastest,
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
            NDIlib_recv_capture_v3(
                recvInstance, &videoFrameHeader, nullptr, nullptr, 100)) {
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

auto NDISource::HandleVideoFrame(const NDIlib_video_frame_v2_t& videoFrame) noexcept -> ErrorCode {

    const auto sourceFormat = SourceFormatFromNDIFourCC(videoFrame.FourCC);

    const auto startTime = std::chrono::steady_clock::now();

    m_renderer.ExecutePipeline(
        videoFrame.p_data,
        Dims2D{(uint32_t)videoFrame.xres, (uint32_t)videoFrame.yres},
        sourceFormat,
        videoFrame.line_stride_in_bytes,
        m_renderSettings);

    const auto endTime = std::chrono::steady_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    m_stats.sourceDims   = Dims2D{static_cast<uint32_t>(videoFrame.xres),
                                static_cast<uint32_t>(videoFrame.yres)};
    m_stats.sourceFormat = sourceFormat;
    m_stats.sourceFPS    = static_cast<float>(videoFrame.frame_rate_N) / static_cast<float>(videoFrame.frame_rate_D);

    m_stats.SetRenderTimings(duration);

    return ErrorCode::None;
}

} // namespace scpp
