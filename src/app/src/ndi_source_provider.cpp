#include "ndi_source_provider.hpp"

namespace scpp {
NDISourceProvider::NDISourceProvider(
    const OpenCLDeviceProvider&         deviceProviderRef,
    std::chrono::steady_clock::duration enumerationInterval)
    : m_deviceProviderRef{deviceProviderRef}
    , m_enumerationInterval{enumerationInterval} {

    const NDIlib_find_create_t findSettings{true, nullptr, nullptr};

    m_findInstance = NDIlib_find_create_v3(&findSettings);

    if (!m_findInstance) {
        std::println("Failed to create NDI find instance.");
        return;
    }

    m_enumerationThread = std::thread([this]() { EnumerationLoop(); });
}

NDISourceProvider::~NDISourceProvider() {

    m_shouldStop = true;

    if (m_enumerationThread.joinable())
        m_enumerationThread.join();

    if (m_findInstance) {
        NDIlib_find_destroy(m_findInstance);
    }
}

void NDISourceProvider::EnumerationLoop() {
    while (!m_shouldStop) {
        uint32_t   noSources = 0;
        const auto sources   = NDIlib_find_get_current_sources(m_findInstance, &noSources);

        if (noSources != m_sources.size()) {
            m_sources.clear();
            m_sources.reserve(noSources);
            for (const auto& source : std::span{sources, noSources}) {
                m_sources.emplace_back(source);
            }
        }

        std::this_thread::sleep_for(m_enumerationInterval);
    }
}

} // namespace scpp
