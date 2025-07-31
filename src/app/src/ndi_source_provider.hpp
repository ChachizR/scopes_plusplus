#pragma once

#include "pch.hpp"
#include "ndi_source.hpp"
#include "error_codes.hpp"

namespace scpp {

class NDISourceProvider {
public:
    NDISourceProvider(
        std::chrono::steady_clock::duration enumerationInterval = 1s);

    ~NDISourceProvider();
private:
    std::chrono::steady_clock::duration m_enumerationInterval;

    NDIlib_find_instance_t m_findInstance = nullptr;


    std::span<const NDIlib_source_t> m_sources;

    std::thread m_enumerationThread;
    bool        m_shouldStop = false;

    void EnumerationLoop();

public:
    [[nodiscard]]
    const std::span<const NDIlib_source_t> GetSources() const noexcept {
        return m_sources;
    }

    [[nodiscard]]
    std::expected<NDISource,ErrorCode> SelectSource(uint32_t index) const noexcept;

    [[nodiscard]]
    std::expected<NDISource, ErrorCode> SelectSource(const NDIlib_source_t& source) const noexcept;
};
} // namespace scpp