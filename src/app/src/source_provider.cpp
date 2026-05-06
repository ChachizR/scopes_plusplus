#include "source_provider.hpp"

#include "test_pattern_source.hpp"

namespace scpp {

SourceProvider::SourceProvider()
    : m_sources{
          SourceRef{
              .name    = "Test Pattern",
              .details = "Built-in animated RGBA source"}} {}

auto SourceProvider::CreateSource(const OpenCLDeviceProvider& deviceProviderRef, const SourceRef& source) const
    -> std::unique_ptr<VideoSource> {
    if (source.name == "Test Pattern") {
        return std::make_unique<TestPatternSource>(deviceProviderRef);
    }

    return nullptr;
}
} // namespace scpp
