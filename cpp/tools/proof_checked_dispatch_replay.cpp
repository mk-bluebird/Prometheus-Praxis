// File: cpp/tools/proof_checked_dispatch_replay.cpp
#include "proof_checked_dispatch_replay.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t kMaximumRiskOfHarmFixed = 300000;

bool IsLowerSnakeCase(const std::string& value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool prior_underscore = false;
    for (const char character : value) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (prior_underscore) {
                return false;
            }
            prior_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        prior_underscore = false;
    }

    return true;
}

bool IsEvidenceStructurallyValid(
    const AuthorizationEvidence& evidence) noexcept {
    return evidence.sequence > 0U &&
           evidence.issue_time_s <= evidence.expiry_time_s &&
           IsLowerSnakeCase(evidence.action_identifier) &&
           IsLowerSnakeCase(evidence.policy_identifier) &&
           evidence.risk_of_harm_fixed >= 0 &&
           evidence.risk_of_harm_fixed <= 1000000;
}

bool SequenceAlreadySeen(const std::vector<std::uint64_t>& sequences,
                         const std::uint64_t sequence) {
    return std::find(sequences.begin(), sequences.end(), sequence) !=
           sequences.end();
}

std::string RejectReason(const AuthorizationReplayRecord& record,
                         const std::string& required_policy,
                         const std::vector<std::uint64_t>& accepted_sequences) {
    const AuthorizationEvidence& evidence = record.evidence;

    if (!IsEvidenceStructurallyValid(evidence)) {
        return "rejected_invalid_evidence";
    }
    if (!evidence.independently_verified) {
        return "rejected_unverified_evidence";
    }
    if (evidence.policy_identifier != required_policy) {
        return "rejected_policy_mismatch";
    }
    if (record.observed_at_s < evidence.issue_time_s ||
        record.observed_at_s > evidence.expiry_time_s) {
        return "rejected_expired_or_not_yet_active";
    }
    if (evidence.risk_of_harm_fixed > kMaximumRiskOfHarmFixed) {
        return "rejected_risk_exceeds_corridor";
    }
    if (SequenceAlreadySeen(accepted_sequences, evidence.sequence)) {
        return "rejected_duplicate_sequence";
    }
    return {};
}

bool IsReplayInternallyConsistent(
    const ProofCheckedDispatchReplay& replay) {
    if (!replay.completed ||
        replay.accepted_count + replay.rejected_count != replay.results.size()) {
        return false;
    }

    for (std::size_t index = 0U; index < replay.results.size(); ++index) {
        if (replay.results[index].record_index != index ||
            replay.results[index].outcome.empty()) {
            return false;
        }
    }

    return true;
}

}  // namespace

ProofCheckedDispatchReplay ReplayProofCheckedDispatch(
    const std::string& policy_identifier,
    const std::vector<AuthorizationReplayRecord>& records) {
    ProofCheckedDispatchReplay replay{};

    if (!IsLowerSnakeCase(policy_identifier)) {
        replay.reasons.emplace_back(
            "policy identifier must be non-empty lower_snake_case");
        return replay;
    }

    std::vector<std::uint64_t> accepted_sequences;
    accepted_sequences.reserve(records.size());
    replay.results.reserve(records.size());

    for (std::size_t index = 0U; index < records.size(); ++index) {
        const AuthorizationReplayRecord& record = records[index];
        const std::string rejection =
            RejectReason(record, policy_identifier, accepted_sequences);

        const bool accepted = rejection.empty();
        if (accepted) {
            accepted_sequences.push_back(record.evidence.sequence);
            ++replay.accepted_count;
        } else {
            ++replay.rejected_count;
        }

        replay.results.push_back(
            AuthorizationReplayResult{
                index,
                accepted,
                accepted ? DescribeAuthorizationReplayOutcome(record, true)
                         : rejection});
    }

    replay.completed = true;
    replay.all_expected_outcomes_match = IsReplayInternallyConsistent(replay);

    if (!replay.all_expected_outcomes_match) {
        replay.reasons.emplace_back(
            "replay result indexing or acceptance counts are inconsistent");
    }

    return replay;
}

std::string DescribeAuthorizationReplayOutcome(
    const AuthorizationReplayRecord& record,
    const bool accepted) {
    std::ostringstream output;
    output << (accepted ? "accepted" : "rejected")
           << "_sequence=" << record.evidence.sequence
           << "; action=" << record.evidence.action_identifier
           << "; policy=" << record.evidence.policy_identifier
           << "; observed_at_s=" << record.observed_at_s;
    return output.str();
}

std::string ExplainProofCheckedDispatchReplay(
    const ProofCheckedDispatchReplay& replay) {
    std::ostringstream output;
    output << "proof_checked_dispatch_replay_completed="
           << (replay.completed ? "1" : "0")
           << "\nall_expected_outcomes_match="
           << (replay.all_expected_outcomes_match ? "1" : "0")
           << "\naccepted_count=" << replay.accepted_count
           << "\nrejected_count=" << replay.rejected_count;

    for (const AuthorizationReplayResult& result : replay.results) {
        output << "\nrecord_index=" << result.record_index
               << "; accepted=" << (result.accepted ? "1" : "0")
               << "; outcome=" << result.outcome;
    }

    for (const std::string& reason : replay.reasons) {
        output << "\nreason=" << reason;
    }

    return output.str();
}

bool ProofCheckedDispatchReplaySelfTest() {
    const std::string policy = "ecological_water_reserve";

    const AuthorizationReplayRecord accepted{
        AuthorizationEvidence{
            1U,
            100U,
            200U,
            "water_quality_observation",
            policy,
            250000,
            true},
        150U};

    const AuthorizationReplayRecord duplicate{
        AuthorizationEvidence{
            1U,
            100U,
            200U,
            "native_habitat_review",
            policy,
            100000,
            true},
        150U};

    const AuthorizationReplayRecord expired{
        AuthorizationEvidence{
            2U,
            100U,
            200U,
            "native_habitat_review",
            policy,
            100000,
            true},
        201U};

    const AuthorizationReplayRecord unverified{
        AuthorizationEvidence{
            3U,
            100U,
            200U,
            "native_habitat_review",
            policy,
            100000,
            false},
        150U};

    const AuthorizationReplayRecord unsafe_risk{
        AuthorizationEvidence{
            4U,
            100U,
            200U,
            "native_habitat_review",
            policy,
            300001,
            true},
        150U};

    const ProofCheckedDispatchReplay replay =
        ReplayProofCheckedDispatch(
            policy,
            {accepted, duplicate, expired, unverified, unsafe_risk});

    if (!replay.completed ||
        !replay.all_expected_outcomes_match ||
        replay.accepted_count != 1U ||
        replay.rejected_count != 4U ||
        replay.results.size() != 5U ||
        !replay.results[0].accepted ||
        replay.results[1].outcome != "rejected_duplicate_sequence" ||
        replay.results[2].outcome != "rejected_expired_or_not_yet_active" ||
        replay.results[3].outcome != "rejected_unverified_evidence" ||
        replay.results[4].outcome != "rejected_risk_exceeds_corridor") {
        return false;
    }

    const ProofCheckedDispatchReplay invalid_policy =
        ReplayProofCheckedDispatch("Invalid-Policy", {accepted});

    if (invalid_policy.completed ||
        invalid_policy.all_expected_outcomes_match ||
        invalid_policy.reasons.empty()) {
        return false;
    }

    return DescribeAuthorizationReplayOutcome(accepted, true) ==
               "accepted_sequence=1; action=water_quality_observation; "
               "policy=ecological_water_reserve; observed_at_s=150" &&
           ExplainProofCheckedDispatchReplay(replay).find(
               "accepted_count=1") != std::string::npos;
}
