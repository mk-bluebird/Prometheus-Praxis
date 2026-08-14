// File: cpp/tools/path_normalization.cpp
#include "path_normalization.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::paths {
namespace {

bool IsControlCharacter(unsigned char value) noexcept {
    return value < 0x20U || value == 0x7FU;
}

bool IsSeparator(char value) noexcept {
    return value == '/' || value == '\\';
}

bool IsDriveQualified(std::string_view value) noexcept {
    return value.size() >= 2U &&
           std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
           value[1] == ':';
}

bool HasUnsafeCharacter(std::string_view value) noexcept {
    for (const unsigned char character : value) {
        if (IsControlCharacter(character) || character == ':') {
            return true;
        }
    }
    return false;
}

bool IsValidComponent(std::string_view component) noexcept {
    if (component.empty() || component == "." || component == "..") {
        return false;
    }

    for (const unsigned char character : component) {
        if (IsControlCharacter(character) ||
            character == ':' ||
            character == '/' ||
            character == '\\') {
            return false;
        }
    }

    return true;
}

std::vector<std::string> SplitNormalizedPath(std::string_view path) {
    std::vector<std::string> components;
    std::size_t start = 0U;

    while (start < path.size()) {
        const std::size_t separator = path.find('/', start);
        const std::size_t end =
            separator == std::string_view::npos ? path.size() : separator;

        components.emplace_back(path.substr(start, end - start));

        if (separator == std::string_view::npos) {
            break;
        }

        start = separator + 1U;
    }

    return components;
}

std::string ExtractExtension(std::string_view filename) {
    const std::size_t dot = filename.rfind('.');
    if (dot == std::string_view::npos ||
        dot == 0U ||
        dot + 1U == filename.size()) {
        return {};
    }

    return std::string(filename.substr(dot));
}

}  // namespace

std::optional<std::string> NormalizeRepositoryPath(std::string_view raw) {
    if (raw.empty() ||
        IsSeparator(raw.front()) ||
        IsSeparator(raw.back()) ||
        IsDriveQualified(raw) ||
        HasUnsafeCharacter(raw)) {
        return std::nullopt;
    }

    std::vector<std::string> components;
    std::string current_component;
    current_component.reserve(raw.size());

    for (const char character : raw) {
        if (IsSeparator(character)) {
            if (!current_component.empty()) {
                if (!IsValidComponent(current_component)) {
                    return std::nullopt;
                }

                components.push_back(std::move(current_component));
                current_component.clear();
            }
            continue;
        }

        current_component.push_back(character);
    }

    if (!IsValidComponent(current_component)) {
        return std::nullopt;
    }

    components.push_back(std::move(current_component));
    return JoinRepositoryPath(components);
}

bool IsRepositoryPathNormalized(std::string_view path) {
    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(path);
    return normalized.has_value() && *normalized == path;
}

std::string DescribeArtifactPathSeparatorStyle(std::string_view path) {
    const bool contains_forward_slash =
        path.find('/') != std::string_view::npos;
    const bool contains_backslash =
        path.find('\\') != std::string_view::npos;

    if (contains_forward_slash && contains_backslash) {
        return "mixed";
    }
    if (contains_forward_slash) {
        return "forward_slash";
    }
    if (contains_backslash) {
        return "backslash";
    }
    return "none";
}

std::string ExplainPathNormalization(std::string_view raw) {
    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(raw);

    if (!normalized.has_value()) {
        return "repository_path=invalid; separator_style=" +
               DescribeArtifactPathSeparatorStyle(raw);
    }

    return "repository_path=valid; separator_style=" +
           DescribeArtifactPathSeparatorStyle(raw) +
           "; normalized=" + *normalized;
}

std::optional<NormalizedPathParts> DecomposeRepositoryPath(
    std::string_view raw) {
    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(raw);
    if (!normalized.has_value()) {
        return std::nullopt;
    }

    NormalizedPathParts parts;
    parts.generic_path = *normalized;
    parts.components = SplitNormalizedPath(parts.generic_path);
    parts.repository_relative = true;

    if (!parts.components.empty()) {
        parts.filename = parts.components.back();
        parts.extension = ExtractExtension(parts.filename);
    }

    return parts;
}

std::optional<std::string> JoinRepositoryPath(
    const std::vector<std::string>& components) {
    if (components.empty()) {
        return std::nullopt;
    }

    std::string joined;
    for (const std::string& component : components) {
        if (!IsValidComponent(component)) {
            return std::nullopt;
        }

        if (!joined.empty()) {
            joined.push_back('/');
        }

        joined.append(component);
    }

    return joined;
}

bool PathNormalizationSelfTest() {
    const auto forward =
        NormalizeRepositoryPath("cpp/tools/foundation_report.cpp");
    const auto backslash =
        NormalizeRepositoryPath("cpp\\tools\\foundation_report.cpp");
    const auto repeated =
        NormalizeRepositoryPath("cpp///tools\\\\foundation_report.cpp");

    if (!forward.has_value() ||
        !backslash.has_value() ||
        !repeated.has_value() ||
        *forward != "cpp/tools/foundation_report.cpp" ||
        *backslash != *forward ||
        *repeated != *forward ||
        !IsRepositoryPathNormalized(*forward) ||
        IsRepositoryPathNormalized("cpp\\tools\\foundation_report.cpp")) {
        return false;
    }

    const std::vector<std::string> unsafe_paths{
        "",
        "/cpp/tools/foundation_report.cpp",
        "\\cpp\\tools\\foundation_report.cpp",
        "C:\\cpp\\tools\\foundation_report.cpp",
        "cpp:tools/foundation_report.cpp",
        "cpp/../tools/foundation_report.cpp",
        "cpp/./tools/foundation_report.cpp",
        "../foundation_report.cpp",
        "cpp/tools/",
        "cpp/tools/\nfoundation_report.cpp",
        "cpp/tools/\x7ffoundation_report.cpp"};

    for (const std::string& path : unsafe_paths) {
        if (NormalizeRepositoryPath(path).has_value()) {
            return false;
        }
    }

    const auto parts =
        DecomposeRepositoryPath("cpp\\tools\\foundation_report.cpp");
    if (!parts.has_value() ||
        parts->generic_path != "cpp/tools/foundation_report.cpp" ||
        parts->components.size() != 3U ||
        parts->components[0] != "cpp" ||
        parts->components[1] != "tools" ||
        parts->filename != "foundation_report.cpp" ||
        parts->extension != ".cpp" ||
        !parts->repository_relative) {
        return false;
    }

    const auto joined = JoinRepositoryPath(
        {"cpp", "eco_restoration", "water_biodiversity_diagnostics.hpp"});
    if (!joined.has_value() ||
        *joined != "cpp/eco_restoration/water_biodiversity_diagnostics.hpp" ||
        JoinRepositoryPath({"cpp", "..", "unsafe.cpp"}).has_value() ||
        JoinRepositoryPath({"cpp", "tools/file.cpp"}).has_value() ||
        JoinRepositoryPath({}).has_value()) {
        return false;
    }

    return DescribeArtifactPathSeparatorStyle("cpp/tools/file.cpp") ==
               "forward_slash" &&
           DescribeArtifactPathSeparatorStyle("cpp\\tools\\file.cpp") ==
               "backslash" &&
           DescribeArtifactPathSeparatorStyle("cpp/tools\\file.cpp") ==
               "mixed" &&
           DescribeArtifactPathSeparatorStyle("file.cpp") == "none" &&
           ExplainPathNormalization("cpp\\tools\\file.cpp") ==
               "repository_path=valid; separator_style=backslash; "
               "normalized=cpp/tools/file.cpp" &&
           ExplainPathNormalization("../unsafe.cpp") ==
               "repository_path=invalid; separator_style=forward_slash";
}

}  // namespace prometheus_praxis::foundation::paths
