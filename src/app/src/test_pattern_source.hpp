#pragma once

#include "pch.hpp"
#include "video_source.hpp"

namespace scpp {

class TestPatternSource : public VideoSource {
private:
    static constexpr Dims2D c_sourceDims{1280u, 720u};
    static constexpr float  c_sourceFPS = 60.f;

    std::vector<uint8_t> m_frameBuffer;

public:
    explicit TestPatternSource(const OpenCLDeviceProvider& deviceProviderRef) noexcept
        : VideoSource{deviceProviderRef}
        , m_frameBuffer(c_sourceDims.Area() * 4u, 0u) {}

    auto Start() -> ErrorCode override;

    [[nodiscard]]
    auto GetName() const noexcept -> std::string_view override {
        return "Test Pattern";
    }

private:
    void RenderLoop();
    void FillFrame(float timeSeconds) noexcept;
};

} // namespace scpp
