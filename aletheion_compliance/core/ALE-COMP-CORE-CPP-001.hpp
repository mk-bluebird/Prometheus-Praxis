// aletheion_compliance/core/ALE-COMP-CORE-CPP-001.hpp
// Central compliance core (C++ interface) for eco_restoration_shard / Prometheus-Praxis.
// Mirrors the Rust ALE-COMP-CORE-001.rs library: blacklist scanning, neurorights checks,
// ecosafety corridor enforcement ("no corridor, no build"), and treaty governance preflight. [file:17]

#ifndef ALE_COMP_CORE_CPP_001_HPP
#define ALE_COMP_CORE_CPP_001_HPP

#include <string>
#include <vector>

namespace aletheion {
namespace compliance {
namespace core {

enum class BlacklistViolationKind {
    None,
    CryptographyBlacklisted,
    DigitalTwinBlacklisted,
    StackBlacklisted,
    Unknown
};

struct BlacklistViolation {
    BlacklistViolationKind kind;
    std::string symbol;
    std::string location;   // file:line or module path.
    std::string rationale;  // human-readable reason.
};

// Neurorights envelope status for FEAR/PAIN/SANITY. [file:17]
struct NeurorightsEnvelopeStatus {
    bool fear_ok;
    bool pain_ok;
    bool sanity_ok;
    std::string detail;     // explanation or empty if OK.
};

// Preflight result for any workflow/module.
struct CompliancePreflightResult {
    bool ok;
    std::vector<BlacklistViolation> blacklist_violations;
    NeurorightsEnvelopeStatus neurorights;
    std::vector<std::string> eco_corridor_violations;
    std::vector<std::string> treaty_violations;
};

// This class is intentionally minimal: all heavy logic lives in Rust,
// but C++ kernels (MRF routing, corridor services) must be able to call it. [file:17]
class CompliancePreflight {
public:
    CompliancePreflight();

    // Run full preflight for the current process/module.
    // Returns structured information; kernels usually gate workflow with result.ok.
    CompliancePreflightResult run() const;

    // Convenience helper for simple gating: returns true if run().ok, false otherwise.
    bool preflight_check() const;

    // Blacklist scanning for specific symbol sets, used by static analyzers or CI.
    std::vector<BlacklistViolation> scan_symbols(
        const std::vector<std::string>& symbols) const;

private:
    // Internal bridge to Rust library (e.g., FFI, extern "C" stubs).
    CompliancePreflightResult run_internal() const;
    std::vector<BlacklistViolation> scan_symbols_internal(
        const std::vector<std::string>& symbols) const;
};

} // namespace core
} // namespace compliance
} // namespace aletheion

#endif // ALE_COMP_CORE_CPP_001_HPP
