#pragma once

#include "pch.hpp"

namespace scpp {

void InitializeRuntimePaths(const std::filesystem::path& executablePath);

[[nodiscard]]
auto GetRuntimeBasePath() noexcept -> const std::filesystem::path&;

[[nodiscard]]
auto ResolveRuntimePath(const std::filesystem::path& relativePath) -> std::filesystem::path;

} // namespace scpp
