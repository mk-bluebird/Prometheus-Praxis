// File: cpp/tools/proof_checked_dispatch_replay.cpp
#include "proof_checked_dispatch_replay.hpp"

#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace prometheus_praxis::foundation::authorization {
namespace {

constexpr std::int32_t kMaximumAcceptedRiskOfHarmFixed = 300000;
constexpr std::int32_t kMaximumRepresentableRiskOfHarmFixed = 1000000;

bool IsLowerSnakeCase(std::string_view value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool prior_underscore = false;
    for (const unsigned char character : value) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        const bool underscore = character == '_';

        if (!lowercase && !digit && !underscore) {
            return false;
        }

        if (underscore && prior_underscore) {
            return false;
        }

        prior_underscore = underscore;
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
           evidence.risk_of_harm_fixed <= kMaximumRepresentableRiskOfHarmFixed;
}

std::string RejectionFor(
    std::string_view expected_policy_identifier,
    const AuthorizationReplayRecord& record,
    const std::set<std::uint64_t>& accepted_sequences) {
    const AuthorizationEvidence& evidence = record.evidence;

    if (!IsLowerSnakeCase(expected_policy_identifier)) {
        return "invalid_policy_identifier";
    }

    if (evidence.sequence == 0U) {
        return "invalid_sequence";
    }

    if (!IsLowerSnakeCase(evidence.action_identifier)) {
        return "invalid_action_identifier";
    }

    if (!IsLowerSnakeCase(evidence.policy_identifier)) {
        return "invalid_evidence_policy_identifier";
    }

    if (evidence.issue_time_s > evidence.expiry_time_s) {
        return "invalid_evidence_time_range";
    }

    if (evidence.risk_of_harm_fixed < 0 ||
        evidence.risk_of_harm_fixed > kMaximumRepresentableRiskOfHarmFixed) {
        return "invalid_risk_of_harm";
    }

    if (!evidence.verified) {
        return "unverified_evidence";
    }

    if (evidence.policy_identifier != expected_policy_identifier) {
        return "policy_mismatch";
    }

    if (accepted_sequences.find(evidence.sequence) != accepted_sequences.end()) {
        return "duplicate_sequence";
    }

    if (record.observed_at_s < evidence.issue_time_s ||
        record.observed_at_s > evidence.expiry_time_s) {
        return "expired_or_not_yet_active";
    }

    if (evidence.risk_of_harm_fixed > kMaximumAcceptedRiskOfHarmFixed) {
        return "risk_exceeds_safety_corridor";
    }

    return {};
}

bool ResultsMatchCounters(
    const ProofCheckedDispatchReplay& replay) noexcept {
    std::size_t accepted = 0U;
    std::size_t rejected = 0U;

    for (std::size_t index = 0U; index < replay.results.size(); ++index) {
        const AuthorizationReplayResult& result = replay.results[index];

        if (result.record_index != index || result.outcome.empty()) {
            return false;
        }

        if (result.accepted) {
            ++accepted;
            if (result.outcome != "accepted") {
                return false;
            }
        } else {
            ++rejected;
            if (result.outcome == "accepted") {
                return false;
            }
        }
    }

    return accepted == replay.accepted_count &&
           rejected == replay.rejected_count &&
           accepted + rejected == replay.results.size();
}

}  // namespace

ProofCheckedDispatchReplay ReplayProofCheckedDispatch(
    std::string_view policy_identifier,
    const std::vector<AuthorizationReplayRecord>& records) {
    ProofCheckedDispatchReplay replay;
    replay.completed = true;
    replay.results.reserve(records.size());
    replay.reasons.reserve(records.size());

    std::set<std::uint64_t> accepted_sequences;

    for (std::size_t index = 0U; index < records.size(); ++index) {
        const AuthorizationReplayRecord& record = records[index];
        const std::string rejection =
            RejectionFor(policy_identifier, record, accepted_sequences);
        const bool accepted = rejection.empty();

        if (accepted) {
            accepted_sequences.insert(record.evidence.sequence);
            ++replay.accepted_count;
        } else {
            ++replay.rejected_count;
            replay.reasons.push_back(
                "record_index=" + std::to_string(index) +
                "; rejection=" + rejection);
        }

        replay.results.push_back(AuthorizationReplayResult{
            index,
            accepted,
            accepted ? "accepted" : rejection});
    }

    replay.all_expected_outcomes_match = IsReplayInternallyConsistent(replay);
    if (!replay.all_expected_outcomes_match) {
        replay.reasons.emplace_back(
            "replay result indexing or acceptance accounting is inconsistent");
    }

    return replay;
}

std::string DescribeAuthorizationReplayOutcome(
    const AuthorizationReplayRecord& record,
    bool accepted) {
    const AuthorizationEvidence& evidence = record.evidence;

    std::ostringstream output;
    output << "authorization_replay_record"
           << "; sequence=" << evidence.sequence
           << "; issue_time_s=" << evidence.issue_time_s
           << "; expiry_time_s=" << evidence.expiry_time_s
           << "; observed_at_s=" << record.observed_at_s
           << "; action_identifier=" << evidence.action_identifier
           << "; policy_identifier=" << evidence.policy_identifier
           << "; verified=" << (evidence.verified ? "true" : "false")
           << "; risk_of_harm_fixed=" << evidence.risk_of_harm_fixed
           << "; structurally_valid="
           << (IsEvidenceStructurallyValid(evidence) ? "true" : "false")
           << "; accepted=" << (accepted ? "true" : "false");
    return output.str();
}

std::string ExplainProofCheckedDispatchReplay(
    const ProofCheckedDispatchReplay& replay) {
    std::ostringstream output;
    output << "proof_checked_dispatch_replay"
           << "; completed=" << (replay.completed ? "true" : "false")
           << "; internally_consistent="
           << (IsReplayInternallyConsistent(replay) ? "true" : "false")
           << "; all_expected_outcomes_match="
           << (replay.all_expected_outcomes_match ? "true" : "false")
           << "; accepted_count=" << replay.accepted_count
           << "; rejected_count=" << replay.rejected_count
           << "; result_count=" << replay.results.size();

    for (const AuthorizationReplayResult& result : replay.results) {
        output << "; record_index=" << result.record_index
               << "; accepted=" << (result.accepted ? "true" : "false")
               << "; outcome=" << result.outcome;
    }

    for (const std::string& reason : replay.reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

bool IsReplayInternallyConsistent(
    const ProofCheckedDispatchReplay& replay) {
    if (!replay.completed) {
        return false;
    }

    return ResultsMatchCounters(replay);
}

bool ProofCheckedDispatchReplaySelfTest() {
    const std::string policy = "ecological_restoration_policy";

    const AuthorizationReplayRecord accepted{
        AuthorizationEvidence{
            1U,
            100U,
            200U,
            "native_planting",
            policy,
            200000,
            true},
        150U};

    const AuthorizationReplayRecord duplicate{
        AuthorizationEvidence{
            1U,
            101U,
            201U,
            "water_release",
            policy,
            200000,
            true},
        150U};

    const AuthorizationReplayRecord expired{
        AuthorizationEvidence{
            2U,
            100U,
            120U,
            "soil_amendment",
            policy,
            200000,
            true},
        121U};

    const AuthorizationReplayRecord unverified{
        AuthorizationEvidence{
            3U,
            100U,
            200U,
            "habitat_monitoring",
            policy,
            200000,
            false},
        150U};

    const AuthorizationReplayRecord policy_mismatch{
        AuthorizationEvidence{
            4U,
            100U,
            200U,
            "water_quality_sampling",
            "different_policy",
            200000,
            true},
        150U};

    const AuthorizationReplayRecord excessive_risk{
        AuthorizationEvidence{
            5U,
            100U,
            200U,
            "invasive_control",
            policy,
            300001,
            true},
        150U};

    const AuthorizationReplayRecord invalid_risk{
        AuthorizationEvidence{
            6U,
            100U,
            200U,
            "soil_amendment",
            policy,
            1000001,
            true},
        150U};

    const ProofCheckedDispatchReplay replay =
        ReplayProofCheckedDispatch(
            policy,
            {
                accepted,
                duplicate,
                expired,
                unverified,
                policy_mismatch,
                excessive_risk,
                invalid_risk,
            });

    if (!replay.completed ||
        !replay.all_expected_outcomes_match ||
        !IsReplayInternallyConsistent(replay) ||
        replay.accepted_count != 1U ||
        replay.rejected_count != 6U ||
        replay.results.size() != 7U ||
        !replay.results[0].accepted ||
        replay.results[1].outcome != "duplicate_sequence" ||
        replay.results[2].outcome != "expired_or_not_yet_active" ||
        replay.results[3].outcome != "unverified_evidence" ||
        replay.results[4].outcome != "policy_mismatch" ||
        replay.results[5].outcome != "risk_exceeds_safety_corridor" ||
        replay.results[6].outcome != "invalid_risk_of_harm") {
        return false;
    }

    const ProofCheckedDispatchReplay invalid_policy =
        ReplayProofCheckedDispatch("Invalid-Policy", {accepted});
    if (!invalid_policy.completed ||
        !invalid_policy.all_expected_outcomes_match ||
        invalid_policy.accepted_count != 0U ||
        invalid_policy.rejected_count != 1U ||
        invalid_policy.results.size() != 1U ||
        invalid_policy.results.front().outcome != "invalid_policy_identifier") {
        return false;
    }

    const ProofCheckedDispatchReplay inconsistent{
        true,
        true,
        1U,
        0U,
        {AuthorizationReplayResult{1U, true, "accepted"}},
        {}};
    if (IsReplayInternallyConsistent(inconsistent)) {
        return false;
    }

    const std::string accepted_description =
        DescribeAuthorizationReplayOutcome(accepted, true);
    const std::string replay_explanation =
        ExplainProofCheckedDispatchReplay(replay);

    return accepted_description.find("accepted=true") != std::string::npos &&
           accepted_description.find("structurally_valid=true") !=
               std::string::npos &&
           replay_explanation.find("outcome=duplicate_sequence") !=
               std::string::npos;
}

}  // namespace prometheus_praxis::foundation::authorization
