// File: cpp/tools/path_normalization.cpp
#include "path_normalization.hpp"

#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

bool ContainsControlCharacter(const std::string_view value) noexcept {
    for (const unsigned char character : value) {
        if (character < 0x20U || character == 0x7fU) {
            return true;
        }
    }
    return false;
}

bool IsDriveQualified(const std::string_view value) noexcept {
    return value.size() >= 2U &&
           ((value[0] >= 'A' && value[0] <= 'Z') ||
            (value[0] >= 'a' && value[0] <= 'z')) &&
           value[1] == ':';
}

bool IsAbsoluteInput(const std::string_view value) noexcept {
    return !value.empty() &&
           (value.front() == '/' || value.front() == '\\' ||
            IsDriveQualified(value));
}

bool ContainsTraversalComponent(const std::string_view value) {
    std::size_t begin = 0U;

    while (begin <= value.size()) {
        const std::size_t separator = value.find_first_of("/\\", begin);
        const std::size_t end = separator == std::string_view::npos
                                    ? value.size()
                                    : separator;

        const std::string_view component = value.substr(begin, end - begin);
        if (component == "..") {
            return true;
        }

        if (separator == std::string_view::npos) {
            break;
        }
        begin = separator + 1U;
    }

    return false;
}

bool IsSafeRawPath(const std::string_view raw) {
    return !raw.empty() &&
           !ContainsControlCharacter(raw) &&
           !IsAbsoluteInput(raw) &&
           !ContainsTraversalComponent(raw);
}

std::string ToGenericSeparators(const std::string_view raw) {
    std::string normalized;
    normalized.reserve(raw.size());

    bool prior_separator = false;
    for (const char character : raw) {
        const bool separator = character == '/' || character == '\\';
        if (separator) {
            if (!prior_separator) {
                normalized.push_back('/');
            }
            prior_separator = true;
        } else {
            normalized.push_back(character);
            prior_separator = false;
        }
    }

    return normalized;
}

bool HasUnsafeNormalizedComponent(const std::filesystem::path& path) {
    for (const std::filesystem::path& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return true;
        }
    }
    return false;
}

}  // namespace

std::optional<std::string> NormalizeRepositoryPath(const std::string_view raw) {
    if (!IsSafeRawPath(raw)) {
        return std::nullopt;
    }

    const std::string generic_input = ToGenericSeparators(raw);
    if (generic_input.empty() || generic_input.back() == '/') {
        return std::nullopt;
    }

    const std::filesystem::path parsed{generic_input};
    if (parsed.empty() || parsed.is_absolute() ||
        HasUnsafeNormalizedComponent(parsed)) {
        return std::nullopt;
    }

    const std::string normalized = parsed.lexically_normal().generic_string();
    if (normalized.empty() ||
        normalized == "." ||
        normalized == ".." ||
        normalized.back() == '/' ||
        normalized.find('\\') != std::string::npos ||
        ContainsTraversalComponent(normalized) ||
        ContainsControlCharacter(normalized)) {
        return std::nullopt;
    }

    return normalized;
}

bool IsRepositoryPathNormalized(const std::string_view path) {
    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(path);

    return normalized.has_value() && *normalized == path;
}

std::string DescribeArtifactPathSeparatorStyle(const std::string_view path) {
    const bool has_forward = path.find('/') != std::string_view::npos;
    const bool has_backward = path.find('\\') != std::string_view::npos;

    if (has_forward && has_backward) {
        return "mixed";
    }
    if (has_forward) {
        return "forward_slash";
    }
    if (has_backward) {
        return "backslash";
    }
    return "none";
}

std::string ExplainPathNormalization(const std::string_view raw) {
    std::ostringstream output;
    output << "path_separator_style="
           << DescribeArtifactPathSeparatorStyle(raw);

    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(raw);

    if (!normalized.has_value()) {
        output << "; normalized=false";
        return output.str();
    }

    output << "; normalized=true; path=" << *normalized;
    return output.str();
}

bool PathNormalizationSelfTest() {
    const std::optional<std::string> forward =
        NormalizeRepositoryPath("cpp/tools/foundation_report.cpp");
    const std::optional<std::string> backward =
        NormalizeRepositoryPath("cpp\\tools\\foundation_report.cpp");
    const std::optional<std::string> repeated =
        NormalizeRepositoryPath("cpp//tools///foundation_report.cpp");

    if (!forward.has_value() ||
        !backward.has_value() ||
        !repeated.has_value() ||
        *forward != "cpp/tools/foundation_report.cpp" ||
        *backward != "cpp/tools/foundation_report.cpp" ||
        *repeated != "cpp/tools/foundation_report.cpp" ||
        !IsRepositoryPathNormalized(*forward) ||
        IsRepositoryPathNormalized("cpp\\tools\\foundation_report.cpp")) {
        return false;
    }

    const std::vector<std::string> unsafe_paths{
        "",
        "/cpp/tools/foundation_report.cpp",
        "\\cpp\\tools\\foundation_report.cpp",
        "C:\\cpp\\tools\\foundation_report.cpp",
        "cpp/../tools/foundation_report.cpp",
        "../foundation_report.cpp",
        "cpp/tools/",
        "cpp/\nfoundation_report.cpp"};

    for (const std::string& path : unsafe_paths) {
        if (NormalizeRepositoryPath(path).has_value()) {
            return false;
        }
    }

    if (DescribeArtifactPathSeparatorStyle("cpp/tools/file.cpp") !=
            "forward_slash" ||
        DescribeArtifactPathSeparatorStyle("cpp\\tools\\file.cpp") !=
            "backslash" ||
        DescribeArtifactPathSeparatorStyle("cpp/tools\\file.cpp") !=
            "mixed" ||
        DescribeArtifactPathSeparatorStyle("file.cpp") != "none") {
        return false;
    }

    return ExplainPathNormalization("cpp\\tools\\file.cpp") ==
               "path_separator_style=backslash; normalized=true; "
               "path=cpp/tools/file.cpp" &&
           ExplainPathNormalization("../unsafe.cpp") ==
               "path_separator_style=forward_slash; normalized=false";
}
