// File: cpp/tools/foundation_source_audit.cpp
#include "foundation_source_audit.hpp"

#include "path_normalization.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::audit {
namespace {

constexpr std::array<std::string_view, 15U> kCanonicalSymbols{
    "FoundationReport",
    "FoundationReportDiff",
    "ValidateFoundationReport",
    "IsFoundationReportValid",
    "DerivedFoundationSafe",
    "CompareFoundationReports",
    "ExplainFoundationReportValidation",
    "ExplainFoundationReportDiff",
    "FoundationReportBuilder",
    "CanonicalExtensionDescriptor",
    "CanonicalExtensionRegistry",
    "RunCanonicalExtensionSelfTests",
    "BuildKnownExtensionRegistry",
    "FoundationExitCode",
    "DispatchUnifiedFoundationCommand",
};

bool IsIdentifierStart(char character) noexcept {
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalpha(value) != 0 || character == '_';
}

bool IsIdentifierCharacter(char character) noexcept {
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_';
}

bool IsValidIdentifier(std::string_view identifier) noexcept {
    if (identifier.empty() || !IsIdentifierStart(identifier.front())) {
        return false;
    }

    return std::all_of(
        identifier.begin() + 1,
        identifier.end(),
        [](char character) { return IsIdentifierCharacter(character); });
}

bool CanonicalSymbolsAreValidAndUnique() {
    std::set<std::string_view> unique_symbols;

    for (const std::string_view symbol : kCanonicalSymbols) {
        if (!IsValidIdentifier(symbol) || !unique_symbols.insert(symbol).second) {
            return false;
        }
    }

    return true;
}

std::size_t CountWholeIdentifierOccurrences(
    std::string_view source,
    std::string_view identifier) {
    std::size_t count = 0U;
    std::size_t position = 0U;

    while (position < source.size()) {
        position = source.find(identifier, position);
        if (position == std::string_view::npos) {
            break;
        }

        const bool valid_left =
            position == 0U || !IsIdentifierCharacter(source[position - 1U]);
        const std::size_t end = position + identifier.size();
        const bool valid_right =
            end == source.size() || !IsIdentifierCharacter(source[end]);

        if (valid_left && valid_right) {
            ++count;
        }

        position = end;
    }

    return count;
}

std::string Trim(std::string_view text) {
    std::size_t first = 0U;
    std::size_t last = text.size();

    while (first < last &&
           std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1U])) != 0) {
        --last;
    }

    return std::string(text.substr(first, last - first));
}

std::vector<std::string> ExtractQuotedIncludes(std::string_view source) {
    std::vector<std::string> includes;
    std::size_t line_start = 0U;

    while (line_start < source.size()) {
        const std::size_t line_end = source.find('\n', line_start);
        const std::size_t end =
            line_end == std::string_view::npos ? source.size() : line_end;
        const std::string line = Trim(source.substr(line_start, end - line_start));

        if (line.starts_with("#include")) {
            const std::size_t opening_quote = line.find('"');
            if (opening_quote != std::string::npos) {
                const std::size_t closing_quote =
                    line.find('"', opening_quote + 1U);
                if (closing_quote != std::string::npos) {
                    includes.emplace_back(
                        line.substr(
                            opening_quote + 1U,
                            closing_quote - opening_quote - 1U));
                }
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1U;
    }

    return includes;
}

bool IsPrefixPath(
    std::string_view path,
    std::string_view prefix) noexcept {
    if (prefix.empty() || !path.starts_with(prefix)) {
        return false;
    }

    return path.size() == prefix.size() ||
           prefix.back() == '/' ||
           path[prefix.size()] == '/';
}

std::optional<std::string> NormalizePath(std::string_view raw) {
    return paths::NormalizeRepositoryPath(raw);
}

}  // namespace

bool FoundationSectionLedgerRegistry::Append(
    FoundationSectionLedgerEntry entry) {
    if (!IsValidIdentifier(entry.identifier) ||
        entry.responsibility.empty() ||
        entry.append_order != entries_.size()) {
        return false;
    }

    const auto duplicate = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&entry](const FoundationSectionLedgerEntry& existing) {
            return existing.identifier == entry.identifier ||
                   existing.append_order == entry.append_order;
        });

    if (duplicate != entries_.end()) {
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
    std::set<std::string> unique_identifiers;

    for (std::size_t index = 0U; index < entries_.size(); ++index) {
        const FoundationSectionLedgerEntry& entry = entries_[index];
        if (!IsValidIdentifier(entry.identifier) ||
            entry.responsibility.empty() ||
            entry.append_order != index ||
            !unique_identifiers.insert(entry.identifier).second) {
            return false;
        }
    }

    return true;
}

std::vector<SymbolAuditResult> AuditCanonicalSymbols(
    std::string_view source) {
    std::vector<SymbolAuditResult> results;
    results.reserve(kCanonicalSymbols.size());

    for (const std::string_view symbol : kCanonicalSymbols) {
        const std::size_t occurrences =
            CountWholeIdentifierOccurrences(source, symbol);
        results.push_back(SymbolAuditResult{
            std::string(symbol),
            occurrences > 0U,
            occurrences});
    }

    return results;
}

bool SourceContainsAllCanonicalSymbols(std::string_view source) {
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

std::vector<ReverseDependencyViolation> DetectReverseDependencies(
    std::string_view source,
    std::string_view tools_prefix,
    std::string_view forbidden_include_prefix) {
    std::vector<ReverseDependencyViolation> violations;
    const std::optional<std::string> normalized_source =
        NormalizePath(tools_prefix);
    const std::optional<std::string> normalized_forbidden_prefix =
        NormalizePath(forbidden_include_prefix);

    if (!normalized_source.has_value() ||
        !normalized_forbidden_prefix.has_value()) {
        return violations;
    }

    if (!IsPrefixPath(*normalized_source, *normalized_forbidden_prefix)) {
        return violations;
    }

    for (const std::string& include : ExtractQuotedIncludes(source)) {
        const std::optional<std::string> normalized_include =
            NormalizePath(include);
        if (!normalized_include.has_value() ||
            !IsPrefixPath(*normalized_include, *normalized_forbidden_prefix)) {
            continue;
        }

        violations.push_back(ReverseDependencyViolation{
            *normalized_source,
            *normalized_include,
            true,
            "quoted include creates forbidden reverse dependency"});
    }

    return violations;
}

std::string ExplainSourceAudit(
    const std::vector<SymbolAuditResult>& symbols,
    const std::vector<ReverseDependencyViolation>& violations) {
    std::ostringstream output;
    output << "foundation_source_audit"
           << "; canonical_symbol_count=" << symbols.size()
           << "; reverse_dependency_violation_count=" << violations.size();

    for (const SymbolAuditResult& result : symbols) {
        output << "; symbol=" << result.symbol
               << "; present=" << (result.present ? "true" : "false")
               << "; occurrences=" << result.occurrences;
    }

    for (const ReverseDependencyViolation& violation : violations) {
        output << "; source_path=" << violation.source_path
               << "; target_path=" << violation.target_path
               << "; forbidden=" << (violation.forbidden ? "true" : "false")
               << "; evidence=" << violation.evidence;
    }

    return output.str();
}

bool FoundationSourceAuditSelfTest() {
    if (!CanonicalSymbolsAreValidAndUnique()) {
        return false;
    }

    std::ostringstream complete_source;
    for (const std::string_view symbol : kCanonicalSymbols) {
        complete_source << "void " << symbol << "();\n";
    }

    const std::vector<SymbolAuditResult> complete =
        AuditCanonicalSymbols(complete_source.str());
    if (!SourceContainsAllCanonicalSymbols(complete_source.str()) ||
        !std::all_of(
            complete.begin(),
            complete.end(),
            [](const SymbolAuditResult& result) {
                return result.present && result.occurrences == 1U;
            })) {
        return false;
    }

    const std::vector<SymbolAuditResult> partial =
        AuditCanonicalSymbols("FoundationReport FoundationReport");
    if (partial.empty() ||
        !partial.front().present ||
        partial.front().occurrences != 2U ||
        SourceContainsAllCanonicalSymbols("FoundationReport")) {
        return false;
    }

    FoundationSectionLedgerRegistry ledger;
    if (!ledger.Append(FoundationSectionLedgerEntry{
            "report_validation",
            "Owns report validation.",
            true,
            true,
            0U}) ||
        !ledger.Append(FoundationSectionLedgerEntry{
            "json_serialization",
            "Owns deterministic JSON serialization.",
            true,
            true,
            1U}) ||
        !ledger.IsSequential() ||
        ledger.Append(FoundationSectionLedgerEntry{
            "duplicate_order",
            "Must be rejected.",
            false,
            false,
            1U})) {
        return false;
    }

    const std::vector<ReverseDependencyViolation> violations =
        DetectReverseDependencies(
            "#include \"cpp/tools/foundation_report.hpp\"\n"
            "#include \"cpp/eco_restoration/soil_health.hpp\"\n",
            "cpp/eco_restoration/water_biodiversity_diagnostics.cpp",
            "cpp/tools");

    if (violations.size() != 1U ||
        violations.front().source_path !=
            "cpp/eco_restoration/water_biodiversity_diagnostics.cpp" ||
        violations.front().target_path !=
            "cpp/tools/foundation_report.hpp") {
        return false;
    }

    const std::vector<ReverseDependencyViolation> allowed =
        DetectReverseDependencies(
            "#include \"cpp/eco_restoration/soil_health.hpp\"\n",
            "cpp/tools/foundation_orchestrator.cpp",
            "cpp/tools");

    if (!allowed.empty()) {
        return false;
    }

    return ExplainSourceAudit(complete, violations).find(
               "reverse_dependency_violation_count=1") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation::audit
