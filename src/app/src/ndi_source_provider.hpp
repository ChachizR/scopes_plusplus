#pragma once

#include "pch.hpp"
#include "ndi_source.hpp"
#include "error_codes.hpp"

namespace scpp {

class NDISourceProvider {
public:
    NDISourceProvider(
        const OpenCLDeviceProvider&         deviceProviderRef,
        std::chrono::steady_clock::duration enumerationInterval = 1s);
       

    ~NDISourceProvider();
private:
    const OpenCLDeviceProvider&         m_deviceProviderRef;

    std::chrono::steady_clock::duration m_enumerationInterval;

    NDIlib_find_instance_t m_findInstance = nullptr;


    std::span<const NDIlib_source_t> m_sources;

    std::thread m_enumerationThread;
    bool        m_shouldStop = false;

    void EnumerationLoop();

public:
    [[nodiscard]]
    auto GetSources() const noexcept -> const std::span<const NDIlib_source_t> {
        return m_sources;
    }

    [[nodiscard]]
    auto SelectSource(const NDIlib_source_t& source) const noexcept -> NDISource {
        return NDISource(m_deviceProviderRef, source);
    }
};
} // namespace scpp