// File: cpp/tools/foundation_safety_semantics.hpp
#pragma once

#include <string>

enum class ThreatContainmentState {
    NotTriggered,
    FailClosed
};

ThreatContainmentState FromLegacyFailClosed(bool legacy_flag) noexcept;

bool ToLegacyFailClosed(ThreatContainmentState state) noexcept;

bool IsSafeContainmentState(ThreatContainmentState state) noexcept;

std::string ExplainContainmentState(ThreatContainmentState state);

bool FoundationSafetySemanticsSelfTest();
