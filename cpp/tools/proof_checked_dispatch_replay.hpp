// File: cpp/tools/proof_checked_dispatch_replay.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::authorization {

struct AuthorizationEvidence {
    std::uint64_t sequence{};
    std::uint64_t issue_time_s{};
    std::uint64_t expiry_time_s{};
    std::string action_identifier;
    std::string policy_identifier;
    std::int32_t risk_of_harm_fixed{};
    bool independently_verified{};
};

struct AuthorizationReplayRecord {
    AuthorizationEvidence evidence;
    std::uint64_t observed_at_s{};
};

struct AuthorizationReplayResult {
    std::size_t record_index{};
    bool accepted{};
    std::string outcome;
};

struct ProofCheckedDispatchReplay {
    bool completed{};
    bool all_expected_outcomes_match{};
    std::size_t accepted_count{};
    std::size_t rejected_count{};
    std::vector<AuthorizationReplayResult> results;
    std::vector<std::string> reasons;
};

ProofCheckedDispatchReplay ReplayProofCheckedDispatch(
    std::string_view policy_identifier,
    const std::vector<AuthorizationReplayRecord>& records);

std::string DescribeAuthorizationReplayOutcome(
    const AuthorizationReplayRecord& record,
    bool accepted);

std::string ExplainProofCheckedDispatchReplay(
    const ProofCheckedDispatchReplay& replay);

bool IsReplayInternallyConsistent(
    const ProofCheckedDispatchReplay& replay);

bool ProofCheckedDispatchReplaySelfTest();

}  // namespace prometheus_praxis::foundation::authorization
