// File: cpp/tools/foundation_safety_semantics.cpp
#include "foundation_safety_semantics.hpp"

#include <cstdint>
#include <string>

namespace prometheus_praxis::foundation::safety {
namespace {

constexpr const char* kLegacyFieldName = "threat_fail_closed";
constexpr const char* kInternalFieldName = "containment_state";
constexpr std::uint32_t kSemanticVersion = 1U;

bool IsRecognizedContainmentState(ThreatContainmentState state) noexcept {
    return state == ThreatContainmentState::NotTriggered ||
           state == ThreatContainmentState::FailClosed;
}

const char* StateName(ThreatContainmentState state) noexcept {
    switch (state) {
        case ThreatContainmentState::NotTriggered:
            return "not_triggered";
        case ThreatContainmentState::FailClosed:
            return "fail_closed";
    }
    return "invalid";
}

const char* StateMeaning(ThreatContainmentState state) noexcept {
    switch (state) {
        case ThreatContainmentState::NotTriggered:
            return "no containment condition is active";
        case ThreatContainmentState::FailClosed:
            return "containment is active and aggregate foundation safety is blocked";
    }
    return "unrecognized containment state blocks aggregate foundation safety";
}

}  // namespace

ThreatContainmentState FromLegacyFailClosed(bool legacy_flag) noexcept {
    return legacy_flag
        ? ThreatContainmentState::FailClosed
        : ThreatContainmentState::NotTriggered;
}

bool ToLegacyFailClosed(ThreatContainmentState state) noexcept {
    return state == ThreatContainmentState::FailClosed;
}

bool IsSafeContainmentState(ThreatContainmentState state) noexcept {
    return state == ThreatContainmentState::NotTriggered;
}

std::string ExplainContainmentState(ThreatContainmentState state) {
    const bool recognized = IsRecognizedContainmentState(state);

    return std::string("legacy_field=") + kLegacyFieldName +
           "; internal_field=" + kInternalFieldName +
           "; containment_state=" + StateName(state) +
           "; recognized=" + (recognized ? "true" : "false") +
           "; safe=" + (IsSafeContainmentState(state) ? "true" : "false") +
           "; meaning=" + StateMeaning(state);
}

ThreatContainmentTransition MakeContainmentTransition(
    bool legacy_fail_closed) noexcept {
    const ThreatContainmentState state =
        FromLegacyFailClosed(legacy_fail_closed);

    return ThreatContainmentTransition{
        legacy_fail_closed,
        state,
        IsSafeContainmentState(state)};
}

bool IsContainmentTransitionSafe(
    const ThreatContainmentTransition& transition) noexcept {
    return IsRecognizedContainmentState(transition.state) &&
           transition.legacy_fail_closed ==
               ToLegacyFailClosed(transition.state) &&
           transition.safe == IsSafeContainmentState(transition.state);
}

std::string ExplainContainmentTransition(
    const ThreatContainmentTransition& transition) {
    return std::string("legacy_field=") + kLegacyFieldName +
           "; legacy_fail_closed=" +
           (transition.legacy_fail_closed ? "true" : "false") +
           "; internal_field=" + kInternalFieldName +
           "; containment_state=" + StateName(transition.state) +
           "; transition_safe=" + (transition.safe ? "true" : "false") +
           "; transition_consistent=" +
           (IsContainmentTransitionSafe(transition) ? "true" : "false") +
           "; meaning=" + StateMeaning(transition.state);
}

bool FoundationSafetySemanticsSelfTest() {
    const ThreatContainmentSemanticVersion version;
    if (version.legacy_field != kLegacyFieldName ||
        version.internal_field != kInternalFieldName ||
        version.version != kSemanticVersion) {
        return false;
    }

    const ThreatContainmentTransition not_triggered =
        MakeContainmentTransition(false);
    if (not_triggered.state != ThreatContainmentState::NotTriggered ||
        !not_triggered.safe ||
        !IsContainmentTransitionSafe(not_triggered) ||
        ToLegacyFailClosed(not_triggered.state)) {
        return false;
    }

    const ThreatContainmentTransition fail_closed =
        MakeContainmentTransition(true);
    if (fail_closed.state != ThreatContainmentState::FailClosed ||
        fail_closed.safe ||
        !IsContainmentTransitionSafe(fail_closed) ||
        !ToLegacyFailClosed(fail_closed.state)) {
        return false;
    }

    for (const bool legacy_value : {false, true}) {
        const ThreatContainmentState state =
            FromLegacyFailClosed(legacy_value);
        if (ToLegacyFailClosed(state) != legacy_value) {
            return false;
        }
    }

    const std::string safe_explanation =
        ExplainContainmentState(ThreatContainmentState::NotTriggered);
    const std::string blocked_explanation =
        ExplainContainmentState(ThreatContainmentState::FailClosed);

    if (safe_explanation.find("legacy_field=threat_fail_closed") ==
            std::string::npos ||
        safe_explanation.find("internal_field=containment_state") ==
            std::string::npos ||
        safe_explanation.find("containment_state=not_triggered") ==
            std::string::npos ||
        safe_explanation.find("safe=true") == std::string::npos ||
        blocked_explanation.find("containment_state=fail_closed") ==
            std::string::npos ||
        blocked_explanation.find("safe=false") == std::string::npos) {
        return false;
    }

    const std::string transition_explanation =
        ExplainContainmentTransition(fail_closed);
    if (transition_explanation.find("legacy_fail_closed=true") ==
            std::string::npos ||
        transition_explanation.find("transition_safe=false") ==
            std::string::npos ||
        transition_explanation.find("transition_consistent=true") ==
            std::string::npos) {
        return false;
    }

    const ThreatContainmentTransition inconsistent{
        false,
        ThreatContainmentState::FailClosed,
        true};

    return !IsContainmentTransitionSafe(inconsistent);
}

}  // namespace prometheus_praxis::foundation::safety
