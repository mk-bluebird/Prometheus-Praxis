// File: cpp/tools/pq_anchor_ndvi_verification.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "../eco_restoration/pq_anchor_ndvi_circuit.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const KeyEpoch retiring_key{
            "eco_anchor_mldsa_epoch_1",
            SignatureFamily::MlDsa,
            1,
            120
        };
        const KeyEpoch replacement_key{
            "eco_anchor_slhdsa_epoch_2",
            SignatureFamily::SlhDsa,
            80,
            240
        };
        const DualSignatureAnchor anchor{
            "phoenix_eco_credit_anchor_20260813",
            "eco_credit_policy_v1",
            95,
            {SignatureFamily::MlDsa, "eco_anchor_mldsa_epoch_1", true},
            {SignatureFamily::SlhDsa, "eco_anchor_slhdsa_epoch_2", true}
        };

        const bool anchor_valid = dual_signature_valid(anchor);
        const bool rotation_valid = key_rotation_is_valid(
            retiring_key, replacement_key, anchor);

        const NdviPublicBounds bounds{
            1000,
            1,
            10000,
            -10000,
            10000
        };
        const NdviPrivateWitness witness{
            200,
            600,
            400,
            800,
            5000
        };
        const bool ndvi_relation_valid = satisfies_fixed_point_ndvi_relation(
            witness, bounds);
        const NdviProofJournal journal = build_ndvi_proof_journal(
            witness, bounds);

        std::cout << std::fixed << std::setprecision(6)
                  << "dual_signature_valid=" << (anchor_valid ? 1 : 0) << '\n'
                  << "key_rotation_valid=" << (rotation_valid ? 1 : 0) << '\n'
                  << "ndvi_relation_valid=" << (ndvi_relation_valid ? 1 : 0) << '\n'
                  << "scaled_ndvi=" << journal.scaled_ndvi << '\n'
                  << "ndvi=" << static_cast<double>(journal.scaled_ndvi) /
                                     static_cast<double>(journal.scale) << '\n'
                  << "ndvi_journal_accepted=" << (journal.within_declared_bounds ? 1 : 0) << '\n'
                  << "knowledge_factor=" << journal.knowledge_factor << '\n'
                  << "eco_impact_value=" << journal.eco_impact_value << '\n';

        return anchor_valid && rotation_valid && ndvi_relation_valid &&
               journal.within_declared_bounds ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "post-quantum anchor and NDVI verification failed: "
                  << error.what() << '\n';
        return 1;
    }
}
