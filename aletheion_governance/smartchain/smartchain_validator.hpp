// aletheion_governance/smartchain/smartchain_validator.hpp
// SMART-Chain Registry validator (C++ interface) for eco_restoration_shard / Prometheus-Praxis.
// Enforces policy-as-code on workflow actions, including PQSTRICT modes and treaty/rights bindings. [file:16]

#ifndef ALETHEION_GOVERNANCE_SMARTCHAIN_VALIDATOR_HPP
#define ALETHEION_GOVERNANCE_SMARTCHAIN_VALIDATOR_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace aletheion {
namespace governance {
namespace smartchain {

// Identifier for a SMART chain, e.g. "SMART01AWPTHERMALTHERMAPHORA". [file:16]
struct SmartChainId {
    std::string value;
};

// Domains attached to a chain (water, thermal, biotic, neurobiome, somatic, equity, etc.). [file:16]
enum class SmartDomain {
    Water,
    Thermal,
    Somatic,
    Biotic,
    Neurobiome,
    Equity,
    Logistics,
    Unknown
};

// PQ cryptography mode requirement per chain. [file:16]
enum class PQMode {
    Classical,
    Hybrid,   // classical + PQ during migration.
    PQStrict  // PQ-only, as required for water/biotic/somatic/neurobiome/equity. [file:16]
};

// Rights grammars and treaties bound to a chain (examples from SMART01, SMART03, SMART04, SMART05). [file:16]
struct ChainBinding {
    std::vector<std::string> treaties;       // e.g., "IndigenousWaterTreaty", "BioticTreaty".
    std::vector<std::string> rights_grammars;// e.g., "RightToShade", "RightToSafeMovement", "LexEthosConsent".
    std::vector<SmartDomain> domains;
    PQMode pq_mode;
};

// Result for validating a single action against a chain.
struct SmartChainValidationResult {
    bool ok;
    SmartChainId chain_id;
    std::string action_label;
    PQMode pq_mode;
    std::vector<std::string> violations; // treaty, rights, domain, or PQ mode violations.
};

// Validator front-end: queries registry, checks PQ mode, domains, and bindings. [file:16]
class SmartChainValidator {
public:
    SmartChainValidator();

    // Validate an action under a given chain with required PQ mode.
    // Common usage from kernels: validate_chain("SMART01...", "WINDNET_DRONE_SORTIE_DOWNTOWN", PQMode::PQStrict).
    SmartChainValidationResult validate_chain(const SmartChainId& chain_id,
                                              const std::string& action_label,
                                              PQMode required_mode) const;

    // Convenience boolean form for simple gating (e.g., corridor services).
    bool validate_chain(const SmartChainId& chain_id,
                        const std::string& action_label,
                        PQMode required_mode,
                        std::vector<std::string>& out_violations) const;

    // Introspection: fetch bindings for a chain (domains, treaties, rights grammars, PQ mode).
    ChainBinding get_chain_binding(const SmartChainId& chain_id) const;

    // Registry presence check (used by CI to ensure new workflows declare chain membership).
    bool has_chain(const SmartChainId& chain_id) const;

private:
    // Internal bridge to Rust registry and validator (smartchainvalidator.rs). [file:16]
    SmartChainValidationResult validate_chain_internal(const SmartChainId& chain_id,
                                                       const std::string& action_label,
                                                       PQMode required_mode) const;

    ChainBinding get_chain_binding_internal(const SmartChainId& chain_id) const;
    bool has_chain_internal(const SmartChainId& chain_id) const;
};

} // namespace smartchain
} // namespace governance
} // namespace aletheion

#endif // ALETHEION_GOVERNANCE_SMARTCHAIN_VALIDATOR_HPP
