// File: cpp/tools/path_normalization.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::paths {

struct NormalizedPathParts {
    std::string generic_path;
    std::vector<std::string> components;
    std::string filename;
    std::string extension;
    bool repository_relative{};
};

std::optional<std::string> NormalizeRepositoryPath(std::string_view raw);

bool IsRepositoryPathNormalized(std::string_view path);

std::string DescribeArtifactPathSeparatorStyle(std::string_view path);

std::string ExplainPathNormalization(std::string_view raw);

std::optional<NormalizedPathParts> DecomposeRepositoryPath(
    std::string_view raw);

std::optional<std::string> JoinRepositoryPath(
    const std::vector<std::string>& components);

bool PathNormalizationSelfTest();

}  // namespace prometheus_praxis::foundation::paths
