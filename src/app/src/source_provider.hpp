#pragma once

#include "pch.hpp"
#include "video_source.hpp"

namespace scpp {

struct SourceRef {
    std::string name;
    std::string details;
};

class OpenCLDeviceProvider;

class SourceProvider {
private:
    std::vector<SourceRef> m_sources;

public:
    SourceProvider();

    [[nodiscard]]
    auto GetSources() const noexcept -> std::span<const SourceRef> {
        return m_sources;
    }

    [[nodiscard]]
    auto CreateSource(const OpenCLDeviceProvider& deviceProviderRef, const SourceRef& source) const
        -> std::unique_ptr<VideoSource>;

};

} // namespace scpp
