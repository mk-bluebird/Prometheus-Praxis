// File: cpp/eco_restoration/pq_anchor_ndvi_circuit.hpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

/*
Dual-signature anchor validity is accepted only when externally verified
signature results are both true:
Valid(M,sigma_L,sigma_P)=Verify_L(M,sigma_L) and Verify_P(M,sigma_P).

Migration invariants:
- Each anchor binds one immutable message, ledger sequence, and policy version.
- The old and replacement public-key identifiers are both recorded.
- The replacement key becomes eligible only after a valid dual-signed anchor.
- Existing valid anchors remain verifiable under their recorded key epochs.
- A key epoch has a non-overlapping activation interval and a declared expiry.
- No signature result, anchor message, or policy identifier may be altered after
  acceptance; invalid anchors are rejected rather than transformed.
*/
enum class SignatureFamily {
    Legacy,
    MlDsa,
    SlhDsa
};

struct SignatureVerification {
    SignatureFamily family{};
    std::string key_identifier;
    bool externally_verified{};
};

struct DualSignatureAnchor {
    std::string message_identifier;
    std::string policy_identifier;
    std::uint64_t ledger_sequence{};
    SignatureVerification legacy_signature;
    SignatureVerification post_quantum_signature;
};

inline bool dual_signature_valid(const DualSignatureAnchor& anchor) {
    return !anchor.message_identifier.empty() &&
           !anchor.policy_identifier.empty() &&
           !anchor.legacy_signature.key_identifier.empty() &&
           !anchor.post_quantum_signature.key_identifier.empty() &&
           anchor.legacy_signature.externally_verified &&
           anchor.post_quantum_signature.externally_verified &&
           anchor.legacy_signature.family != anchor.post_quantum_signature.family;
}

struct KeyEpoch {
    std::string key_identifier;
    SignatureFamily family{};
    std::uint64_t activation_sequence{};
    std::uint64_t expiry_sequence{};
};

inline bool key_rotation_is_valid(const KeyEpoch& retiring,
                                  const KeyEpoch& replacement,
                                  const DualSignatureAnchor& anchor) {
    if (retiring.key_identifier.empty() || replacement.key_identifier.empty() ||
        retiring.expiry_sequence < retiring.activation_sequence ||
        replacement.expiry_sequence < replacement.activation_sequence ||
        replacement.activation_sequence <= retiring.activation_sequence) {
        return false;
    }

    return dual_signature_valid(anchor) &&
           anchor.legacy_signature.key_identifier == retiring.key_identifier &&
           anchor.post_quantum_signature.key_identifier == replacement.key_identifier &&
           anchor.ledger_sequence >= replacement.activation_sequence &&
           anchor.ledger_sequence <= retiring.expiry_sequence &&
           anchor.ledger_sequence <= replacement.expiry_sequence;
}

/*
Private fixed-point NDVI circuit relation.

Witness:
  R, NIR, n=NIR-R, d=NIR+R, q=round(S*n/d).

Public:
  reflectance_upper_bound B, denominator_minimum d_min, scale S,
  reported NDVI q/S, and pass/fail bounds.

Range constraints:
  0<=R<=B, 0<=NIR<=B,
  -B<=n<=B, d_min<=d<=2B, d>0,
  -S<=q<=S.

Nearest-integer quotient constraint with ties rounded upward:
  -d <= 2*(S*n-q*d) < d.

The verifier receives only q and public bounds after a separate proof system
establishes the witness relation; raw Red and NIR bands remain private.
*/
struct NdviPublicBounds {
    std::int64_t reflectance_upper_bound{};
    std::int64_t denominator_minimum{};
    std::int64_t scale{};
    std::int64_t output_minimum{};
    std::int64_t output_maximum{};
};

struct NdviPrivateWitness {
    std::int64_t red{};
    std::int64_t nir{};
    std::int64_t numerator{};
    std::int64_t denominator{};
    std::int64_t scaled_ndvi{};
};

struct NdviProofJournal {
    std::int64_t scaled_ndvi{};
    std::int64_t scale{};
    std::int64_t output_minimum{};
    std::int64_t output_maximum{};
    bool within_declared_bounds{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline bool satisfies_fixed_point_ndvi_relation(
    const NdviPrivateWitness& witness, const NdviPublicBounds& bounds) {
    if (bounds.reflectance_upper_bound <= 0 || bounds.denominator_minimum <= 0 ||
        bounds.scale <= 0 || bounds.output_minimum > bounds.output_maximum) {
        throw std::invalid_argument("invalid public NDVI circuit bounds");
    }

    const std::int64_t band_maximum = bounds.reflectance_upper_bound;
    if (witness.red < 0 || witness.nir < 0 ||
        witness.red > band_maximum || witness.nir > band_maximum ||
        witness.numerator < -band_maximum || witness.numerator > band_maximum ||
        witness.denominator < bounds.denominator_minimum ||
        witness.denominator > 2 * band_maximum ||
        witness.scaled_ndvi < -bounds.scale || witness.scaled_ndvi > bounds.scale) {
        return false;
    }

    const __int128 expected_numerator =
        static_cast<__int128>(witness.nir) - static_cast<__int128>(witness.red);
    const __int128 expected_denominator =
        static_cast<__int128>(witness.nir) + static_cast<__int128>(witness.red);
    if (expected_numerator != witness.numerator ||
        expected_denominator != witness.denominator) {
        return false;
    }

    const __int128 residual =
        static_cast<__int128>(bounds.scale) * witness.numerator -
        static_cast<__int128>(witness.scaled_ndvi) * witness.denominator;
    const __int128 doubled_residual = 2 * residual;
    return doubled_residual >= -static_cast<__int128>(witness.denominator) &&
           doubled_residual < static_cast<__int128>(witness.denominator);
}

inline NdviProofJournal build_ndvi_proof_journal(
    const NdviPrivateWitness& witness, const NdviPublicBounds& bounds) {
    const bool relation_holds = satisfies_fixed_point_ndvi_relation(witness, bounds);
    const bool output_in_range = witness.scaled_ndvi >= bounds.output_minimum &&
                                 witness.scaled_ndvi <= bounds.output_maximum;
    const bool accepted = relation_holds && output_in_range;

    const double normalized_ndvi = static_cast<double>(witness.scaled_ndvi) /
                                   static_cast<double>(bounds.scale);
    const double knowledge = accepted ? 0.95 : 0.0;
    const double impact = accepted
        ? std::clamp((normalized_ndvi + 1.0) * 0.5, 0.0, 1.0)
        : 0.0;

    return {witness.scaled_ndvi, bounds.scale, bounds.output_minimum,
            bounds.output_maximum, accepted, knowledge, impact};
}

}  // namespace eco_restoration
