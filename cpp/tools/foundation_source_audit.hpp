// File: cpp/tools/foundation_source_audit.hpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::audit {

struct SymbolAuditResult {
    std::string symbol;
    bool present{};
    std::size_t occurrences{};
};

struct FoundationSectionLedgerEntry {
    std::string identifier;
    std::string responsibility;
    bool self_tested{};
    bool registered{};
    std::size_t append_order{};
};

struct ReverseDependencyViolation {
    std::string source_path;
    std::string target_path;
    bool forbidden{true};
    std::string evidence;
};

class FoundationSectionLedgerRegistry {
public:
    bool Append(FoundationSectionLedgerEntry entry);

    const std::vector<FoundationSectionLedgerEntry>& Entries() const noexcept;

    bool IsSequential() const noexcept;

private:
    std::vector<FoundationSectionLedgerEntry> entries_;
};

std::vector<SymbolAuditResult> AuditCanonicalSymbols(
    std::string_view source);

bool SourceContainsAllCanonicalSymbols(std::string_view source);

std::vector<ReverseDependencyViolation> DetectReverseDependencies(
    std::string_view source,
    std::string_view source_path,
    std::string_view forbidden_include_prefix);

std::string ExplainSourceAudit(
    const std::vector<SymbolAuditResult>& symbols,
    const std::vector<ReverseDependencyViolation>& violations);

bool FoundationSourceAuditSelfTest();

}  // namespace prometheus_praxis::foundation::audit
