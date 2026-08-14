// File: cpp/tools/path_normalization.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>

std::optional<std::string> NormalizeRepositoryPath(std::string_view raw);

bool IsRepositoryPathNormalized(std::string_view path);

std::string ExplainPathNormalization(std::string_view raw);

std::string DescribeArtifactPathSeparatorStyle(std::string_view path);

bool PathNormalizationSelfTest();
