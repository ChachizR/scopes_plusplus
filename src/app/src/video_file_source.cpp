#include "video_file_source.hpp"

#include "runtime_paths.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace scpp {

namespace {

class ScopedPacket {
public:
    AVPacket* value{av_packet_alloc()};
    ~ScopedPacket() {
        if (value != nullptr) {
            av_packet_free(&value);
        }
    }
};

class ScopedFrame {
public:
    AVFrame* value{av_frame_alloc()};
    ~ScopedFrame() {
        if (value != nullptr) {
            av_frame_free(&value);
        }
    }
};

[[nodiscard]]
auto FFmpegErrorString(int errnum) -> std::string {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    av_strerror(errnum, buffer.data(), buffer.size());
    return std::string{buffer.data()};
}

[[nodiscard]]
auto URLScheme(std::string_view input) noexcept -> std::string_view {
    const auto schemeEnd = input.find("://"sv);
    if (schemeEnd == std::string_view::npos || schemeEnd == 0u) {
        return {};
    }

    const auto scheme = input.substr(0u, schemeEnd);
    const bool validScheme = std::ranges::all_of(scheme, [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '+' || ch == '-' || ch == '.';
    });
    return validScheme ? scheme : std::string_view{};
}

[[nodiscard]]
auto IsStreamURL(std::string_view input) noexcept -> bool {
    return !URLScheme(input).empty();
}

[[nodiscard]]
auto IsSRTURL(std::string_view input) noexcept -> bool {
    return URLScheme(input) == "srt"sv;
}

[[nodiscard]]
auto IsPlaylistPath(const std::filesystem::path& path) noexcept -> bool {
    const auto extension = path.extension().string();
    return extension == ".m3u8" || extension == ".m3u";
}

[[nodiscard]]
auto SourceDisplayName(const std::filesystem::path& path) -> std::string {
    const auto text = path.string();
    if (IsSRTURL(text)) {
        return text;
    }

    return path.filename().empty() ? text : path.filename().string();
}

void InitFFmpegNetwork() {
    static const int initialized = []() {
        avformat_network_init();
        return 1;
    }();
    (void)initialized;
}

} // namespace

VideoFileSource::VideoFileSource(const OpenCLDeviceProvider& deviceProviderRef, std::filesystem::path path) noexcept
    : VideoSource{deviceProviderRef}
    , m_path{std::move(path)}
    , m_name{SourceDisplayName(m_path)} {}

auto VideoFileSource::Start() -> ErrorCode {
    if (m_isRunning) {
        return ErrorCode::SourceAlreadyRunning;
    }

    InitFFmpegNetwork();

    const bool urlInput = IsStreamURL(m_path.string());
    if (urlInput && !IsSRTURL(m_path.string())) {
        std::println("Unsupported stream URL '{}'. Only SRT URLs are currently supported.", m_path.string());
        return ErrorCode::SourceNotFound;
    }

    if (!urlInput && IsPlaylistPath(m_path)) {
        std::println("Unsupported playlist source '{}'. HLS/playlist playback is disabled.", m_path.string());
        return ErrorCode::SourceNotFound;
    }

    if (!urlInput) {
        const auto resolvedPath = ResolveRuntimePath(m_path);
        if (!std::filesystem::exists(resolvedPath)) {
            std::println("Video file not found: {}", resolvedPath.string());
            return ErrorCode::FSNotFound;
        }

        m_path = resolvedPath;
    }

    m_name       = SourceDisplayName(m_path);
    m_decodedSequence = 0u;
    m_lastRenderedSequence = 0u;
    m_droppedDecodedFrames = 0u;
    m_latestDecodedFrame = {};
    m_stagedFrame = {};
    m_isRunning  = true;
    m_shouldStop = false;
    m_thread     = std::thread([this]() { DecodeLoop(); });

    return ErrorCode::None;
}

void VideoFileSource::UpdateOnMainThread() noexcept {
    float stagedSourceFPS = 0.0f;

    {
        std::scoped_lock lock(m_frameMutex);
        m_stats.decodedFrameCount = m_decodedSequence;
        m_stats.queuedFrameCount  = 0u;
        stagedSourceFPS           = m_nominalSourceFPS;

        if (!m_latestDecodedFrame.ready || m_latestDecodedFrame.sequence == m_lastRenderedSequence) {
            return;
        }

        m_stagedFrame.dims            = m_latestDecodedFrame.dims;
        m_stagedFrame.lineStrideBytes = m_latestDecodedFrame.lineStrideBytes;
        m_stagedFrame.sequence        = m_latestDecodedFrame.sequence;
        m_stagedFrame.ready           = true;
        m_stagedFrame.data.swap(m_latestDecodedFrame.data);
        m_latestDecodedFrame.ready = false;
    }

    if (!m_stagedFrame.ready || m_stagedFrame.data.empty()) {
        return;
    }

    auto frameRenderSettings = m_renderSettings;
    const auto analysisFrameInterval = (std::max)(1u, frameRenderSettings.analysisFrameInterval);
    const bool updateAnalysisThisFrame = (m_stagedFrame.sequence % analysisFrameInterval) == 0u;
    if (!updateAnalysisThisFrame) {
        frameRenderSettings.enabledFeatures = RenderFeature::None;
    }

    const auto renderStart = Clock::now();

    m_renderer.ExecutePipeline(
        m_stagedFrame.data.data(),
        m_stagedFrame.dims,
        SourceFormat::BGRA_8888,
        m_stagedFrame.lineStrideBytes,
        frameRenderSettings);

    const auto renderEnd = Clock::now();

    m_stats.sourceDims   = m_stagedFrame.dims;
    m_stats.sourceFPS    = stagedSourceFPS;
    m_stats.sourceFormat = SourceFormat::BGRA_8888;
    m_stats.SetRenderTimings(renderEnd - renderStart);

    m_lastRenderedSequence = m_stagedFrame.sequence;
    m_stats.renderedFrameCount = m_lastRenderedSequence;
    m_stats.droppedFrameCount = m_droppedDecodedFrames;
}

void VideoFileSource::DecodeLoop() {
    AVFormatContext* formatContext = nullptr;
    AVDictionary*    openOptions   = nullptr;

    const bool streamURL = IsStreamURL(m_path.string());
    if (streamURL) {
        // Values are in microseconds. They keep dead feeds from blocking forever.
        av_dict_set(&openOptions, "rw_timeout", "5000000", 0);
    }

    if (const auto openResult = avformat_open_input(&formatContext, m_path.string().c_str(), nullptr, streamURL ? &openOptions : nullptr); openResult < 0) {
        av_dict_free(&openOptions);
        std::println("Failed to open video file '{}': {}", m_path.string(), FFmpegErrorString(openResult));
        m_isRunning = false;
        return;
    }

    av_dict_free(&openOptions);

    auto formatContextDeleter = [](AVFormatContext* context) {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    };
    std::unique_ptr<AVFormatContext, decltype(formatContextDeleter)> formatGuard(formatContext, formatContextDeleter);

    if (const auto streamInfoResult = avformat_find_stream_info(formatContext, nullptr); streamInfoResult < 0) {
        std::println("Failed to read stream info '{}': {}", m_path.string(), FFmpegErrorString(streamInfoResult));
        m_isRunning = false;
        return;
    }

    const auto videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        std::println("No video stream found in '{}': {}", m_path.string(), FFmpegErrorString(videoStreamIndex));
        m_isRunning = false;
        return;
    }

    AVStream* const videoStream = formatContext->streams[videoStreamIndex];
    const bool canLoop = !streamURL && (formatContext->pb == nullptr || (formatContext->pb->seekable & AVIO_SEEKABLE_NORMAL));
    const bool shouldPaceDecode = canLoop;
    const AVCodec*   codec      = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (codec == nullptr) {
        std::println("No decoder found for '{}'", m_path.string());
        m_isRunning = false;
        return;
    }

    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (codecContext == nullptr) {
        std::println("Failed to allocate decoder context for '{}'", m_path.string());
        m_isRunning = false;
        return;
    }

    auto codecContextDeleter = [](AVCodecContext* context) {
        if (context != nullptr) {
            avcodec_free_context(&context);
        }
    };
    std::unique_ptr<AVCodecContext, decltype(codecContextDeleter)> codecGuard(codecContext, codecContextDeleter);

    if (const auto paramsResult = avcodec_parameters_to_context(codecContext, videoStream->codecpar); paramsResult < 0) {
        std::println("Failed to copy codec parameters '{}': {}", m_path.string(), FFmpegErrorString(paramsResult));
        m_isRunning = false;
        return;
    }

    if (const auto openCodecResult = avcodec_open2(codecContext, codec, nullptr); openCodecResult < 0) {
        std::println("Failed to open decoder '{}': {}", m_path.string(), FFmpegErrorString(openCodecResult));
        m_isRunning = false;
        return;
    }

    const auto srcDims = Dims2D{
        static_cast<uint32_t>(codecContext->width),
        static_cast<uint32_t>(codecContext->height)};

    auto frameRate = av_q2d(videoStream->avg_frame_rate);
    if (frameRate <= 0.0) {
        frameRate = av_q2d(videoStream->r_frame_rate);
    }
    if (frameRate <= 0.0) {
        frameRate = 30.0;
    }
    {
        std::scoped_lock lock(m_frameMutex);
        m_nominalSourceFPS = static_cast<float>(frameRate);
    }
    std::println("Opened video file '{}': {}x{} {:.3f} fps",
                 m_path.string(),
                 srcDims.width,
                 srcDims.height,
                 frameRate);

    SwsContext* swsContext = sws_getContext(
        codecContext->width,
        codecContext->height,
        codecContext->pix_fmt,
        codecContext->width,
        codecContext->height,
        AV_PIX_FMT_BGRA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (swsContext == nullptr) {
        std::println("Failed to create swscale context for '{}'", m_path.string());
        m_isRunning = false;
        return;
    }

    std::unique_ptr<SwsContext, decltype(&sws_freeContext)> swsGuard(swsContext, &sws_freeContext);

    std::vector<uint8_t> convertedFrame(static_cast<size_t>(codecContext->width) * static_cast<size_t>(codecContext->height) * 4u);
    uint8_t*             dstData[4]     = {convertedFrame.data(), nullptr, nullptr, nullptr};
    int                  dstLinesize[4] = {codecContext->width * 4, 0, 0, 0};

    ScopedPacket packet;
    ScopedFrame  frame;

    if (packet.value == nullptr || frame.value == nullptr) {
        std::println("Failed to allocate FFmpeg frame state for '{}'", m_path.string());
        m_isRunning = false;
        return;
    }

    const auto frameDuration = std::chrono::duration<double>{1.0 / frameRate};
    auto       nextFrameTime = Clock::now();

    auto processDecodedFrames = [&]() -> bool {
        while (!m_shouldStop) {
            const auto receiveResult = avcodec_receive_frame(codecContext, frame.value);
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return true;
            }
            if (receiveResult < 0) {
                std::println("Failed to decode video frame '{}': {}", m_path.string(), FFmpegErrorString(receiveResult));
                return false;
            }

            const auto convertStart = Clock::now();
            sws_scale(
                swsContext,
                frame.value->data,
                frame.value->linesize,
                0,
                codecContext->height,
                dstData,
                dstLinesize);
            const auto convertEnd = Clock::now();

            {
                const auto copyStart = Clock::now();
                std::scoped_lock lock(m_frameMutex);
                PendingFrame decodedFrame;
                decodedFrame.dims            = srcDims;
                decodedFrame.lineStrideBytes = static_cast<uint32_t>(dstLinesize[0]);
                decodedFrame.ready           = true;
                decodedFrame.sequence        = ++m_decodedSequence;
                decodedFrame.data            = convertedFrame;

                if (m_latestDecodedFrame.ready) {
                    ++m_droppedDecodedFrames;
                }
                m_latestDecodedFrame = std::move(decodedFrame);

                m_stats.queuedFrameCount = 0u;
                m_stats.SetDecodeTimings(convertEnd - convertStart, Clock::now() - copyStart);
            }

            if (shouldPaceDecode) {
                nextFrameTime += std::chrono::duration_cast<Clock::duration>(frameDuration);
                const auto now = Clock::now();
                if (nextFrameTime > now) {
                    std::this_thread::sleep_until(nextFrameTime);
                } else {
                    nextFrameTime = now;
                }
            }
        }

        return true;
    };

    while (!m_shouldStop) {
        const auto readResult = av_read_frame(formatContext, packet.value);

        if (readResult == AVERROR_EOF) {
            avcodec_send_packet(codecContext, nullptr);
            if (!processDecodedFrames()) {
                break;
            }

            if (!canLoop) {
                break;
            }

            avcodec_flush_buffers(codecContext);
            if (const auto seekResult = av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD); seekResult < 0) {
                std::println("Failed to loop video '{}': {}", m_path.string(), FFmpegErrorString(seekResult));
                break;
            }
            nextFrameTime = Clock::now();
            continue;
        }

        if (readResult < 0) {
            std::println("Failed to read video frame '{}': {}", m_path.string(), FFmpegErrorString(readResult));
            break;
        }

        if (packet.value->stream_index != videoStreamIndex) {
            av_packet_unref(packet.value);
            continue;
        }

        const auto sendResult = avcodec_send_packet(codecContext, packet.value);
        av_packet_unref(packet.value);

        if (sendResult < 0) {
            std::println("Failed to send packet to decoder '{}': {}", m_path.string(), FFmpegErrorString(sendResult));
            break;
        }

        if (!processDecodedFrames()) {
            break;
        }
    }

    m_isRunning = false;
}

} // namespace scpp
