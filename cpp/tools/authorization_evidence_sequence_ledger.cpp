// File: cpp/tools/authorization_evidence_sequence_ledger.cpp
#include "authorization_evidence_sequence_ledger.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t kMinimumRiskOfHarmFixed = 0;
constexpr std::int32_t kMaximumRiskOfHarmFixed = 1000000;

bool IsLowerSnakeCase(const std::string& value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const char character : value) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (previous_underscore) {
                return false;
            }
            previous_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        previous_underscore = false;
    }

    return true;
}

bool IsEntryStructurallyValid(
    const AuthorizationSequenceLedgerEntry& entry) noexcept {
    return entry.sequence > 0U &&
           entry.issue_time_s <= entry.expiry_time_s &&
           IsLowerSnakeCase(entry.action_identifier) &&
           IsLowerSnakeCase(entry.policy_identifier) &&
           entry.risk_of_harm_fixed >= kMinimumRiskOfHarmFixed &&
           entry.risk_of_harm_fixed <= kMaximumRiskOfHarmFixed;
}

bool HasSequence(
    const std::vector<AuthorizationSequenceLedgerEntry>& entries,
    const std::uint64_t sequence) {
    return std::any_of(
        entries.begin(),
        entries.end(),
        [sequence](const AuthorizationSequenceLedgerEntry& entry) {
            return entry.sequence == sequence;
        });
}

std::vector<AuthorizationSequenceLedgerEntry> OrderedEntries(
    const AuthorizationEvidenceSequenceLedger& ledger) {
    std::vector<AuthorizationSequenceLedgerEntry> ordered =
        ledger.Entries();

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const AuthorizationSequenceLedgerEntry& left,
           const AuthorizationSequenceLedgerEntry& right) {
            return left.sequence < right.sequence;
        });

    return ordered;
}

}  // namespace

bool AuthorizationEvidenceSequenceLedger::RecordAccepted(
    AuthorizationSequenceLedgerEntry evidence) {
    if (!IsEntryStructurallyValid(evidence) ||
        HasSequence(entries_, evidence.sequence)) {
        return false;
    }

    entries_.push_back(std::move(evidence));
    return true;
}

const std::vector<AuthorizationSequenceLedgerEntry>&
AuthorizationEvidenceSequenceLedger::Entries() const noexcept {
    return entries_;
}

AuthorizationEvidenceSequenceLedgerAudit
AuditAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    const std::uint64_t observed_at_s) {
    AuthorizationEvidenceSequenceLedgerAudit audit{};
    const std::vector<AuthorizationSequenceLedgerEntry> ordered =
        OrderedEntries(ledger);

    if (ordered.empty()) {
        audit.reasons.emplace_back(
            "authorization evidence ledger contains no accepted entries");
        return audit;
    }

    audit.structurally_valid = true;
    audit.monotonic = true;
    audit.contiguous = true;
    audit.active_at_observation = true;

    std::uint64_t previous_sequence = 0U;
    for (std::size_t index = 0U; index < ordered.size(); ++index) {
        const AuthorizationSequenceLedgerEntry& entry = ordered[index];

        if (!IsEntryStructurallyValid(entry)) {
            audit.structurally_valid = false;
            audit.reasons.emplace_back(
                "authorization evidence entry violates structural constraints");
        }

        if (index > 0U) {
            if (entry.sequence <= previous_sequence) {
                audit.monotonic = false;
                audit.reasons.emplace_back(
                    "authorization evidence sequence is not strictly monotonic");
            }

            if (previous_sequence == UINT64_MAX ||
                entry.sequence != previous_sequence + 1U) {
                audit.contiguous = false;
                audit.reasons.emplace_back(
                    "authorization evidence sequence contains a gap");
            }
        }

        if (observed_at_s < entry.issue_time_s ||
            observed_at_s > entry.expiry_time_s) {
            audit.active_at_observation = false;
            audit.reasons.emplace_back(
                "authorization evidence is inactive at observation time");
        }

        previous_sequence = entry.sequence;
    }

    audit.accepted = audit.structurally_valid &&
                     audit.monotonic &&
                     audit.contiguous &&
                     audit.active_at_observation;
    return audit;
}

std::string ExplainAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    const AuthorizationEvidenceSequenceLedgerAudit& audit) {
    std::ostringstream output;
    output << "authorization_evidence_entries=" << ledger.Entries().size()
           << "\nstructurally_valid=" << (audit.structurally_valid ? "1" : "0")
           << "\nmonotonic=" << (audit.monotonic ? "1" : "0")
           << "\ncontiguous=" << (audit.contiguous ? "1" : "0")
           << "\nactive_at_observation="
           << (audit.active_at_observation ? "1" : "0")
           << "\naccepted=" << (audit.accepted ? "1" : "0");

    for (const std::string& reason : audit.reasons) {
        output << "\nreason=" << reason;
    }

    return output.str();
}

bool AuthorizationEvidenceSequenceLedgerSelfTest() {
    AuthorizationEvidenceSequenceLedger clean;

    if (!clean.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                1U,
                100U,
                200U,
                "water_quality_observation",
                "ecological_water_reserve",
                200000}) ||
        !clean.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                2U,
                100U,
                200U,
                "native_habitat_review",
                "biodiversity_protection",
                100000})) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit clean_audit =
        AuditAuthorizationEvidenceSequenceLedger(clean, 150U);

    if (!clean_audit.accepted ||
        !clean_audit.structurally_valid ||
        !clean_audit.monotonic ||
        !clean_audit.contiguous ||
        !clean_audit.active_at_observation) {
        return false;
    }

    if (clean.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                1U,
                100U,
                200U,
                "duplicate_sequence",
                "ecological_water_reserve",
                100000})) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger gap;
    if (!gap.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                1U,
                100U,
                200U,
                "water_quality_observation",
                "ecological_water_reserve",
                200000}) ||
        !gap.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                3U,
                100U,
                200U,
                "native_habitat_review",
                "biodiversity_protection",
                100000})) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit gap_audit =
        AuditAuthorizationEvidenceSequenceLedger(gap, 150U);
    if (gap_audit.accepted || gap_audit.contiguous) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit expired_audit =
        AuditAuthorizationEvidenceSequenceLedger(clean, 201U);
    if (expired_audit.accepted || expired_audit.active_at_observation) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger invalid;
    if (invalid.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                0U,
                100U,
                200U,
                "invalid_sequence",
                "ecological_water_reserve",
                100000}) ||
        invalid.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                1U,
                200U,
                100U,
                "invalid_window",
                "ecological_water_reserve",
                100000}) ||
        invalid.RecordAccepted(
            AuthorizationSequenceLedgerEntry{
                1U,
                100U,
                200U,
                "invalid_risk",
                "ecological_water_reserve",
                1000001})) {
        return false;
    }

    return ExplainAuthorizationEvidenceSequenceLedger(clean, clean_audit).find(
               "accepted=1") != std::string::npos;
}
