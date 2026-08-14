// File: cpp/tools/authorization_evidence_sequence_ledger.cpp
#include "authorization_evidence_sequence_ledger.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::authorization {
namespace {

constexpr std::int32_t kMinimumRiskOfHarmFixed = 0;
constexpr std::int32_t kMaximumRiskOfHarmFixed = 1000000;

bool IsLowerSnakeCase(std::string_view value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const unsigned char character : value) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        const bool underscore = character == '_';

        if (!lowercase && !digit && !underscore) {
            return false;
        }
        if (underscore && previous_underscore) {
            return false;
        }
        previous_underscore = underscore;
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
    std::uint64_t sequence) noexcept {
    return std::any_of(
        entries.begin(),
        entries.end(),
        [sequence](const AuthorizationSequenceLedgerEntry& entry) {
            return entry.sequence == sequence;
        });
}

std::vector<AuthorizationSequenceLedgerEntry> OrderedEntries(
    const AuthorizationEvidenceSequenceLedger& ledger) {
    std::vector<AuthorizationSequenceLedgerEntry> ordered = ledger.Entries();
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const AuthorizationSequenceLedgerEntry& left,
           const AuthorizationSequenceLedgerEntry& right) {
            return left.sequence < right.sequence;
        });
    return ordered;
}

void AddReason(
    AuthorizationEvidenceSequenceLedgerAudit& audit,
    std::string reason) {
    audit.reasons.push_back(std::move(reason));
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

std::size_t AuthorizationEvidenceSequenceLedger::Size() const noexcept {
    return entries_.size();
}

std::optional<AuthorizationSequenceLedgerEntry>
AuthorizationEvidenceSequenceLedger::Latest() const {
    if (entries_.empty()) {
        return std::nullopt;
    }

    const auto latest = std::max_element(
        entries_.begin(),
        entries_.end(),
        [](const AuthorizationSequenceLedgerEntry& left,
           const AuthorizationSequenceLedgerEntry& right) {
            return left.sequence < right.sequence;
        });

    return *latest;
}

AuthorizationEvidenceSequenceLedgerAudit
AuditAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    std::uint64_t observed_at_s) {
    AuthorizationEvidenceSequenceLedgerAudit audit;
    const std::vector<AuthorizationSequenceLedgerEntry> ordered =
        OrderedEntries(ledger);

    if (ordered.empty()) {
        AddReason(audit, "ledger contains no accepted authorization evidence");
        return audit;
    }

    audit.structurally_valid = true;
    audit.monotonic = true;
    audit.contiguous = true;
    audit.active_at_observation = true;

    std::set<std::uint64_t> observed_sequences;
    std::uint64_t previous_sequence = 0U;
    bool first = true;

    for (const AuthorizationSequenceLedgerEntry& entry : ordered) {
        if (!IsEntryStructurallyValid(entry)) {
            audit.structurally_valid = false;
            AddReason(
                audit,
                "invalid entry at sequence " + std::to_string(entry.sequence));
        }

        if (!observed_sequences.insert(entry.sequence).second) {
            audit.monotonic = false;
            audit.contiguous = false;
            AddReason(
                audit,
                "duplicate sequence " + std::to_string(entry.sequence));
        }

        if (!first) {
            if (entry.sequence <= previous_sequence) {
                audit.monotonic = false;
                AddReason(
                    audit,
                    "sequence is not strictly increasing at " +
                        std::to_string(entry.sequence));
            }

            if (previous_sequence ==
                    std::numeric_limits<std::uint64_t>::max() ||
                entry.sequence != previous_sequence + 1U) {
                audit.contiguous = false;
                AddReason(
                    audit,
                    "sequence gap between " +
                        std::to_string(previous_sequence) +
                        " and " + std::to_string(entry.sequence));
            }
        }

        if (observed_at_s < entry.issue_time_s ||
            observed_at_s > entry.expiry_time_s) {
            audit.active_at_observation = false;
            AddReason(
                audit,
                "entry sequence " + std::to_string(entry.sequence) +
                    " is inactive at observation time");
        }

        previous_sequence = entry.sequence;
        first = false;
    }

    audit.accepted =
        audit.structurally_valid &&
        audit.monotonic &&
        audit.contiguous &&
        audit.active_at_observation;
    return audit;
}

std::string ExplainAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    const AuthorizationEvidenceSequenceLedgerAudit& audit) {
    std::ostringstream output;
    output << "authorization_evidence_sequence_ledger"
           << "; entry_count=" << ledger.Size()
           << "; structurally_valid="
           << (audit.structurally_valid ? "true" : "false")
           << "; monotonic=" << (audit.monotonic ? "true" : "false")
           << "; contiguous=" << (audit.contiguous ? "true" : "false")
           << "; active_at_observation="
           << (audit.active_at_observation ? "true" : "false")
           << "; accepted=" << (audit.accepted ? "true" : "false");

    for (const AuthorizationSequenceLedgerEntry& entry : OrderedEntries(ledger)) {
        output << "; sequence=" << entry.sequence
               << "; issue_time_s=" << entry.issue_time_s
               << "; expiry_time_s=" << entry.expiry_time_s
               << "; action_identifier=" << entry.action_identifier
               << "; policy_identifier=" << entry.policy_identifier
               << "; risk_of_harm_fixed=" << entry.risk_of_harm_fixed;
    }

    for (const std::string& reason : audit.reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

AuthorizationEvidenceSequenceLedger MergeLedgers(
    const AuthorizationEvidenceSequenceLedger& left,
    const AuthorizationEvidenceSequenceLedger& right) {
    AuthorizationEvidenceSequenceLedger merged;

    for (const AuthorizationSequenceLedgerEntry& entry : OrderedEntries(left)) {
        static_cast<void>(merged.RecordAccepted(entry));
    }

    for (const AuthorizationSequenceLedgerEntry& entry : OrderedEntries(right)) {
        static_cast<void>(merged.RecordAccepted(entry));
    }

    return merged;
}

bool AuthorizationEvidenceSequenceLedgerSelfTest() {
    const AuthorizationSequenceLedgerEntry first{
        1U,
        100U,
        200U,
        "water_release",
        "ecological_reserve_policy",
        200000};

    const AuthorizationSequenceLedgerEntry second{
        2U,
        110U,
        210U,
        "native_planting",
        "habitat_regeneration_policy",
        150000};

    AuthorizationEvidenceSequenceLedger clean;
    if (!clean.RecordAccepted(first) ||
        !clean.RecordAccepted(second) ||
        clean.Size() != 2U ||
        clean.RecordAccepted(first)) {
        return false;
    }

    const std::optional<AuthorizationSequenceLedgerEntry> latest = clean.Latest();
    if (!latest.has_value() || latest->sequence != 2U) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit clean_audit =
        AuditAuthorizationEvidenceSequenceLedger(clean, 150U);
    if (!clean_audit.structurally_valid ||
        !clean_audit.monotonic ||
        !clean_audit.contiguous ||
        !clean_audit.active_at_observation ||
        !clean_audit.accepted ||
        !clean_audit.reasons.empty()) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger gap;
    if (!gap.RecordAccepted(first) ||
        !gap.RecordAccepted(AuthorizationSequenceLedgerEntry{
            3U,
            100U,
            200U,
            "soil_amendment",
            "soil_health_policy",
            100000})) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit gap_audit =
        AuditAuthorizationEvidenceSequenceLedger(gap, 150U);
    if (gap_audit.contiguous || gap_audit.accepted) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger expired;
    if (!expired.RecordAccepted(first)) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit expired_audit =
        AuditAuthorizationEvidenceSequenceLedger(expired, 201U);
    if (expired_audit.active_at_observation || expired_audit.accepted) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger invalid;
    if (invalid.RecordAccepted(AuthorizationSequenceLedgerEntry{
            0U,
            100U,
            200U,
            "invalid_sequence",
            "ecological_reserve_policy",
            100000}) ||
        invalid.RecordAccepted(AuthorizationSequenceLedgerEntry{
            1U,
            200U,
            100U,
            "invalid_window",
            "ecological_reserve_policy",
            100000}) ||
        invalid.RecordAccepted(AuthorizationSequenceLedgerEntry{
            1U,
            100U,
            200U,
            "invalid_risk",
            "ecological_reserve_policy",
            kMaximumRiskOfHarmFixed + 1})) {
        return false;
    }

    AuthorizationEvidenceSequenceLedger right;
    if (!right.RecordAccepted(AuthorizationSequenceLedgerEntry{
            3U,
            120U,
            220U,
            "soil_amendment",
            "soil_health_policy",
            100000}) ||
        !right.RecordAccepted(second)) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedger merged =
        MergeLedgers(clean, right);
    if (merged.Size() != 3U) {
        return false;
    }

    const AuthorizationEvidenceSequenceLedgerAudit merged_audit =
        AuditAuthorizationEvidenceSequenceLedger(merged, 150U);
    if (!merged_audit.accepted) {
        return false;
    }

    const std::string explanation =
        ExplainAuthorizationEvidenceSequenceLedger(clean, clean_audit);
    return explanation.find("entry_count=2") != std::string::npos &&
           explanation.find("sequence=1") != std::string::npos &&
           explanation.find("sequence=2") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation::authorization
