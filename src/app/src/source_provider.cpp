#include "source_provider.hpp"

#include "test_pattern_source.hpp"
#include "video_file_source.hpp"

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

auto SourceProvider::CreateVideoFileSource(const OpenCLDeviceProvider& deviceProviderRef, const std::filesystem::path& path) const
    -> std::unique_ptr<VideoSource> {
    return std::make_unique<VideoFileSource>(deviceProviderRef, path);
}

auto SourceProvider::CreateVideoFileSource(const std::filesystem::path& path) const
    -> std::unique_ptr<VideoSource> {
    return std::make_unique<VideoFileSource>(path);
}

} // namespace scpp
