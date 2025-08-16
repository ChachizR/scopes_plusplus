#pragma once

#include "pch.hpp"
#include "ndi_source.hpp"
#include "utils.hpp"

namespace scpp {

struct NDISourceRef {
    std::string name;
    std::string urlAddress;

    NDISourceRef(const NDIlib_source_t& source) noexcept
        : name{source.p_ndi_name ? source.p_ndi_name : "Unknown"}
        , urlAddress{source.p_url_address ? source.p_url_address : "Unknown"} {}
    
};

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


    std::vector<NDISourceRef> m_sources;

    std::thread m_enumerationThread;
    bool        m_shouldStop = false;

    void EnumerationLoop();

public:
    [[nodiscard]]
    auto GetSources() const noexcept -> const std::span<const NDISourceRef> {
        return m_sources;
    }

    [[nodiscard]]
    auto SelectSource(const NDIlib_source_t& source) const noexcept -> NDISource {
        return NDISource(m_deviceProviderRef, source);
    }
};
} // namespace scpp