// File: cpp/tools/foundation_source_audit.cpp
#include "foundation_source_audit.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

const std::vector<std::string>& CanonicalSymbols() {
    static const std::vector<std::string> symbols{
        "FoundationReport",
        "FoundationReportDiff",
        "ValidateFoundationReport",
        "CompareFoundationReports",
        "IsFoundationReportValid",
        "ExplainFoundationReportValidation",
        "ExplainFoundationReportDiff",
        "CanonicalExtensionDescriptor",
        "CanonicalExtensionRegistry",
        "RunCanonicalExtensionSelfTests",
        "BuildKnownExtensionRegistry",
        "FoundationExitCode",
        "DispatchUnifiedFoundationCommand",
        "BuildUnifiedFoundationCommandRegistry",
        "NormalizeRepositoryPath"};
    return symbols;
}

bool IsValidSymbolName(const std::string_view symbol) noexcept {
    if (symbol.empty()) {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(symbol.front());
    if (!(std::isalpha(first) != 0 || symbol.front() == '_')) {
        return false;
    }

    for (const char character : symbol) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (std::isalnum(value) == 0 && character != '_') {
            return false;
        }
    }

    return true;
}

bool CanonicalSymbolsAreValidAndUnique() {
    const std::vector<std::string>& symbols = CanonicalSymbols();

    for (std::size_t index = 0U; index < symbols.size(); ++index) {
        if (!IsValidSymbolName(symbols[index])) {
            return false;
        }
        for (std::size_t other = index + 1U; other < symbols.size(); ++other) {
            if (symbols[index] == symbols[other]) {
                return false;
            }
        }
    }

    return true;
}

std::size_t CountOccurrences(const std::string_view source,
                             const std::string_view needle) {
    if (needle.empty()) {
        return 0U;
    }

    std::size_t count = 0U;
    std::size_t position = 0U;
    while (position < source.size()) {
        position = source.find(needle, position);
        if (position == std::string_view::npos) {
            break;
        }
        ++count;
        position += needle.size();
    }

    return count;
}

bool IsRelativeGenericPath(const std::string_view path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\') {
        return false;
    }

    if (path.find('\\') != std::string_view::npos ||
        path.find(':') != std::string_view::npos) {
        return false;
    }

    const std::filesystem::path filesystem_path{std::string(path)};
    if (filesystem_path.is_absolute()) {
        return false;
    }

    for (const std::filesystem::path& component : filesystem_path) {
        if (component == "." || component == ".." || component.empty()) {
            return false;
        }
    }

    return true;
}

}  // namespace

std::vector<SymbolAuditResult> AuditCanonicalSymbols(
    const std::string_view source) {
    std::vector<SymbolAuditResult> results;
    results.reserve(CanonicalSymbols().size());

    for (const std::string& symbol : CanonicalSymbols()) {
        const std::size_t occurrences = CountOccurrences(source, symbol);
        results.push_back(SymbolAuditResult{
            symbol,
            occurrences > 0U,
            occurrences});
    }

    return results;
}

bool SourceContainsAllCanonicalSymbols(const std::string_view source) {
    if (!CanonicalSymbolsAreValidAndUnique()) {
        return false;
    }

    const std::vector<SymbolAuditResult> results =
        AuditCanonicalSymbols(source);

    return std::all_of(
        results.begin(),
        results.end(),
        [](const SymbolAuditResult& result) {
            return result.present;
        });
}

std::optional<std::string> NormalizeRepositoryPath(
    const std::string_view raw_path) {
    if (!IsRelativeGenericPath(raw_path)) {
        return std::nullopt;
    }

    const std::filesystem::path raw{std::string(raw_path)};
    const std::filesystem::path normalized = raw.lexically_normal();
    const std::string generic = normalized.generic_string();

    if (!IsRelativeGenericPath(generic) || generic.empty()) {
        return std::nullopt;
    }

    return generic;
}

bool IsRepositoryPathNormalized(const std::string_view path) {
    const std::optional<std::string> normalized =
        NormalizeRepositoryPath(path);

    return normalized.has_value() && *normalized == path;
}

std::vector<std::string> HeaderSelfSufficiencyRequirements() {
    return {
        "<algorithm>",
        "<cstddef>",
        "<cmath>",
        "<filesystem>",
        "<functional>",
        "<limits>",
        "<numeric>",
        "<optional>",
        "<sstream>",
        "<string>",
        "<string_view>",
        "<vector>"};
}

bool FoundationSectionLedgerRegistry::Append(
    FoundationSectionLedgerEntry entry) {
    if (entry.identifier.empty() ||
        entry.responsibility.empty() ||
        entry.append_order != entries_.size() + 1U) {
        return false;
    }

    const bool duplicate_identifier = std::any_of(
        entries_.begin(),
        entries_.end(),
        [&entry](const FoundationSectionLedgerEntry& existing) {
            return existing.identifier == entry.identifier;
        });

    if (duplicate_identifier) {
        return false;
    }

    entries_.push_back(std::move(entry));
    return true;
}

const std::vector<FoundationSectionLedgerEntry>&
FoundationSectionLedgerRegistry::Entries() const noexcept {
    return entries_;
}

bool FoundationSectionLedgerRegistry::IsSequential() const noexcept {
    for (std::size_t index = 0U; index < entries_.size(); ++index) {
        const FoundationSectionLedgerEntry& entry = entries_[index];
        if (entry.identifier.empty() ||
            entry.responsibility.empty() ||
            entry.append_order != index + 1U) {
            return false;
        }
    }
    return true;
}

bool FoundationSourceAuditSelfTest() {
    if (!CanonicalSymbolsAreValidAndUnique()) {
        return false;
    }

    std::ostringstream complete_source;
    for (const std::string& symbol : CanonicalSymbols()) {
        complete_source << symbol << '\n';
    }

    if (!SourceContainsAllCanonicalSymbols(complete_source.str())) {
        return false;
    }

    const std::string partial_source =
        "FoundationReport\nValidateFoundationReport\n";
    if (SourceContainsAllCanonicalSymbols(partial_source)) {
        return false;
    }

    const std::vector<SymbolAuditResult> partial_results =
        AuditCanonicalSymbols(partial_source);
    const auto report_result = std::find_if(
        partial_results.begin(),
        partial_results.end(),
        [](const SymbolAuditResult& result) {
            return result.symbol == "FoundationReport";
        });

    if (report_result == partial_results.end() ||
        !report_result->present ||
        report_result->occurrences != 1U) {
        return false;
    }

    const std::optional<std::string> valid_path =
        NormalizeRepositoryPath("cpp/tools/foundation_report.cpp");
    if (!valid_path.has_value() ||
        *valid_path != "cpp/tools/foundation_report.cpp" ||
        !IsRepositoryPathNormalized(*valid_path)) {
        return false;
    }

    const std::vector<std::string> invalid_paths{
        "",
        "/cpp/tools/foundation_report.cpp",
        "../foundation_report.cpp",
        "cpp/../foundation_report.cpp",
        "cpp\\tools\\foundation_report.cpp",
        "C:/foundation_report.cpp"};

    for (const std::string& path : invalid_paths) {
        if (NormalizeRepositoryPath(path).has_value() ||
            IsRepositoryPathNormalized(path)) {
            return false;
        }
    }

    const std::vector<std::string> requirements =
        HeaderSelfSufficiencyRequirements();
    if (std::find(
            requirements.begin(),
            requirements.end(),
            "<numeric>") == requirements.end()) {
        return false;
    }

    FoundationSectionLedgerRegistry ledger;
    if (!ledger.Append(
            FoundationSectionLedgerEntry{
                "foundation_report_validator",
                "Validates foundation diagnostic reports.",
                true,
                true,
                1U}) ||
        !ledger.Append(
            FoundationSectionLedgerEntry{
                "canonical_extension_registry",
                "Runs ordered diagnostic extension self-tests.",
                true,
                true,
                2U}) ||
        !ledger.IsSequential()) {
        return false;
    }

    if (ledger.Append(
            FoundationSectionLedgerEntry{
                "foundation_report_validator",
                "Duplicate identifiers are rejected.",
                false,
                false,
                3U}) ||
        ledger.Append(
            FoundationSectionLedgerEntry{
                "foundation_command_dispatcher",
                "Out-of-order records are rejected.",
                true,
                false,
                4U})) {
        return false;
    }

    return ledger.Entries().size() == 2U;
}
