// File: cpp/tools/authorization_evidence_sequence_ledger.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AuthorizationSequenceLedgerEntry {
    std::uint64_t sequence{};
    std::uint64_t issue_time_s{};
    std::uint64_t expiry_time_s{};
    std::string action_identifier;
    std::string policy_identifier;
    std::int32_t risk_of_harm_fixed{};
};

struct AuthorizationEvidenceSequenceLedgerAudit {
    bool structurally_valid{};
    bool monotonic{};
    bool contiguous{};
    bool active_at_observation{};
    bool accepted{};
    std::vector<std::string> reasons;
};

class AuthorizationEvidenceSequenceLedger {
public:
    bool RecordAccepted(AuthorizationSequenceLedgerEntry evidence);

    const std::vector<AuthorizationSequenceLedgerEntry>& Entries() const noexcept;

private:
    std::vector<AuthorizationSequenceLedgerEntry> entries_;
};

AuthorizationEvidenceSequenceLedgerAudit
AuditAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    std::uint64_t observed_at_s);

std::string ExplainAuthorizationEvidenceSequenceLedger(
    const AuthorizationEvidenceSequenceLedger& ledger,
    const AuthorizationEvidenceSequenceLedgerAudit& audit);

bool AuthorizationEvidenceSequenceLedgerSelfTest();
