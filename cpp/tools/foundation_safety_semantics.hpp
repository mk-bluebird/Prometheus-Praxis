// File: cpp/tools/foundation_safety_semantics.hpp
#pragma once

#include <cstdint>
#include <string>

namespace prometheus_praxis::foundation::safety {

enum class ThreatContainmentState : std::uint8_t {
    NotTriggered = 0U,
    FailClosed = 1U
};

struct ThreatContainmentSemanticVersion {
    std::string legacy_field{"threat_fail_closed"};
    std::string internal_field{"containment_state"};
    std::uint32_t version{1U};
};

struct ThreatContainmentTransition {
    bool legacy_fail_closed{};
    ThreatContainmentState state{ThreatContainmentState::NotTriggered};
    bool safe{true};
};

[[nodiscard]] ThreatContainmentState FromLegacyFailClosed(
    bool legacy_flag) noexcept;

[[nodiscard]] bool ToLegacyFailClosed(
    ThreatContainmentState state) noexcept;

[[nodiscard]] bool IsSafeContainmentState(
    ThreatContainmentState state) noexcept;

[[nodiscard]] std::string ExplainContainmentState(
    ThreatContainmentState state);

[[nodiscard]] ThreatContainmentTransition MakeContainmentTransition(
    bool legacy_fail_closed) noexcept;

[[nodiscard]] bool IsContainmentTransitionSafe(
    const ThreatContainmentTransition& transition) noexcept;

[[nodiscard]] std::string ExplainContainmentTransition(
    const ThreatContainmentTransition& transition);

[[nodiscard]] bool FoundationSafetySemanticsSelfTest();

}  // namespace prometheus_praxis::foundation::safety
