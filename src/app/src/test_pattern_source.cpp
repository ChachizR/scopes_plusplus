#include "test_pattern_source.hpp"

namespace scpp {

namespace {

constexpr float c_pi = 3.14159265358979323846f;

[[nodiscard]]
auto ToByte(float value) noexcept -> uint8_t {
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

} // namespace

auto TestPatternSource::Start() -> ErrorCode {
    if (m_isRunning) {
        return ErrorCode::SourceAlreadyRunning;
    }

    m_isRunning  = true;
    m_shouldStop = false;
    m_thread     = std::thread([this]() { RenderLoop(); });

    return ErrorCode::None;
}

void TestPatternSource::RenderLoop() {
    const auto frameInterval = std::chrono::duration<double>{1.0 / c_sourceFPS};
    auto       nextFrameTime = Clock::now();
    auto       startTime     = Clock::now();

    while (!m_shouldStop) {
        const auto now         = Clock::now();
        const auto timeSeconds = std::chrono::duration<float>(now - startTime).count();

        FillFrame(timeSeconds);

        const auto renderStart = Clock::now();

        m_renderer.ExecutePipeline(
            m_frameBuffer.data(),
            c_sourceDims,
            SourceFormat::RGBA_8888,
            static_cast<uint32_t>(c_sourceDims.width * 4u),
            m_renderSettings);

        const auto renderEnd = Clock::now();

        m_stats.sourceDims   = c_sourceDims;
        m_stats.sourceFPS    = c_sourceFPS;
        m_stats.sourceFormat = SourceFormat::RGBA_8888;
        m_stats.SetRenderTimings(renderEnd - renderStart);

        nextFrameTime += std::chrono::duration_cast<Clock::duration>(frameInterval);
        std::this_thread::sleep_until(nextFrameTime);
    }
}

void TestPatternSource::FillFrame(float timeSeconds) noexcept {
    constexpr std::array<glm::vec3, 8> bars{
        glm::vec3{1.0f, 1.0f, 1.0f},
        glm::vec3{1.0f, 1.0f, 0.0f},
        glm::vec3{0.0f, 1.0f, 1.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        glm::vec3{1.0f, 0.0f, 1.0f},
        glm::vec3{1.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 1.0f},
        glm::vec3{0.1f, 0.1f, 0.1f},
    };

    const auto width  = static_cast<float>(c_sourceDims.width);
    const auto height = static_cast<float>(c_sourceDims.height);

    for (uint32_t y = 0; y < c_sourceDims.height; ++y) {
        for (uint32_t x = 0; x < c_sourceDims.width; ++x) {
            const auto nx = static_cast<float>(x) / width;
            const auto ny = static_cast<float>(y) / height;

            glm::vec3 color{};

            if (ny < 0.65f) {
                const auto barIndex = std::min<size_t>(static_cast<size_t>(nx * bars.size()), bars.size() - 1u);
                color               = bars[barIndex];
            } else if (ny < 0.85f) {
                const auto wave =
                    0.5f + 0.5f * std::sin((nx * 18.0f + timeSeconds * 1.75f) * c_pi);
                color = glm::vec3{wave, nx, 1.0f - nx};
            } else {
                const auto centerX = 0.5f + 0.25f * std::cos(timeSeconds * 0.9f);
                const auto centerY = 0.5f + 0.20f * std::sin(timeSeconds * 1.2f);
                const auto dx      = nx - centerX;
                const auto dy      = ny - centerY;
                const auto dist    = std::sqrt(dx * dx + dy * dy);
                const auto ring    = 0.5f + 0.5f * std::cos(90.0f * dist - timeSeconds * 6.0f);
                color              = glm::vec3{ring, 1.0f - ring, ny};
            }

            const auto idx      = static_cast<size_t>((y * c_sourceDims.width + x) * 4u);
            m_frameBuffer[idx]  = ToByte(color.r * 255.0f);
            m_frameBuffer[idx + 1u] = ToByte(color.g * 255.0f);
            m_frameBuffer[idx + 2u] = ToByte(color.b * 255.0f);
            m_frameBuffer[idx + 3u] = 255u;
        }
    }
}

} // namespace scpp
