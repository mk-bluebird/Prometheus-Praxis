// File: cpp/tools/foundation_source_audit.hpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

std::vector<SymbolAuditResult> AuditCanonicalSymbols(
    std::string_view source);

bool SourceContainsAllCanonicalSymbols(std::string_view source);

std::optional<std::string> NormalizeRepositoryPath(
    std::string_view raw_path);

bool IsRepositoryPathNormalized(std::string_view path);

std::vector<std::string> HeaderSelfSufficiencyRequirements();

class FoundationSectionLedgerRegistry {
public:
    bool Append(FoundationSectionLedgerEntry entry);

    const std::vector<FoundationSectionLedgerEntry>& Entries() const noexcept;

    bool IsSequential() const noexcept;

private:
    std::vector<FoundationSectionLedgerEntry> entries_;
};

bool FoundationSourceAuditSelfTest();
