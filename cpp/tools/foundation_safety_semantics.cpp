// File: cpp/tools/foundation_safety_semantics.cpp
#include "foundation_safety_semantics.hpp"

#include <string>

ThreatContainmentState FromLegacyFailClosed(
    const bool legacy_flag) noexcept {
    return legacy_flag
               ? ThreatContainmentState::FailClosed
               : ThreatContainmentState::NotTriggered;
}

bool ToLegacyFailClosed(const ThreatContainmentState state) noexcept {
    return state == ThreatContainmentState::FailClosed;
}

bool IsSafeContainmentState(
    const ThreatContainmentState state) noexcept {
    return state == ThreatContainmentState::NotTriggered;
}

std::string ExplainContainmentState(
    const ThreatContainmentState state) {
    switch (state) {
        case ThreatContainmentState::NotTriggered:
            return "threat_containment_state=not_triggered; "
                   "no fail-closed containment condition is active";
        case ThreatContainmentState::FailClosed:
            return "threat_containment_state=fail_closed; "
                   "containment is active and the foundation safety decision "
                   "is blocked";
    }

    return "threat_containment_state=invalid; "
           "the foundation safety decision is blocked";
}

bool FoundationSafetySemanticsSelfTest() {
    const ThreatContainmentState safe_state =
        FromLegacyFailClosed(false);
    const ThreatContainmentState blocked_state =
        FromLegacyFailClosed(true);

    if (safe_state != ThreatContainmentState::NotTriggered ||
        blocked_state != ThreatContainmentState::FailClosed) {
        return false;
    }

    if (ToLegacyFailClosed(safe_state) ||
        !ToLegacyFailClosed(blocked_state)) {
        return false;
    }

    if (!IsSafeContainmentState(safe_state) ||
        IsSafeContainmentState(blocked_state)) {
        return false;
    }

    if (ExplainContainmentState(safe_state) !=
            "threat_containment_state=not_triggered; "
            "no fail-closed containment condition is active" ||
        ExplainContainmentState(blocked_state) !=
            "threat_containment_state=fail_closed; "
            "containment is active and the foundation safety decision "
            "is blocked") {
        return false;
    }

    return true;
}
