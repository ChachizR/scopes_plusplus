#include "runtime_paths.hpp"

namespace scpp {

namespace {

std::filesystem::path g_runtimeBasePath = std::filesystem::current_path();

} // namespace

void InitializeRuntimePaths(const std::filesystem::path& executablePath) {
    if (executablePath.empty()) {
        g_runtimeBasePath = std::filesystem::current_path();
        return;
    }

    std::error_code ec;
    const auto canonicalExecutablePath = std::filesystem::weakly_canonical(executablePath, ec);

    if (ec) {
        g_runtimeBasePath = executablePath.parent_path();
        return;
    }

    g_runtimeBasePath = canonicalExecutablePath.parent_path();
}

auto GetRuntimeBasePath() noexcept -> const std::filesystem::path& {
    return g_runtimeBasePath;
}

auto ResolveRuntimePath(const std::filesystem::path& relativePath) -> std::filesystem::path {
    if (relativePath.is_absolute()) {
        return relativePath;
    }

    return g_runtimeBasePath / relativePath;
}

} // namespace scpp
