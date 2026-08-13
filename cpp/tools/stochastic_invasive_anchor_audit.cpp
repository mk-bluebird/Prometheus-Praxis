// File: cpp/tools/stochastic_invasive_anchor_audit.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/stochastic_invasive_and_anchor_audit.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const StochasticPopulationModel population_model{
            48.0,
            1.0,
            0.08,
            0.015,
            0.20,
            0.60,
            0.35,
            0.08
        };
        const std::vector<InvasiveControlCandidate> controls{
            {0.10, 4.0, 5.0, 0.12},
            {0.25, 7.0, 10.0, 0.18},
            {0.45, 14.0, 12.0, 0.24},
            {0.60, 22.0, 16.0, 0.34}
        };

        const StochasticControlDecision control =
            select_safe_stochastic_invasive_control(
                population_model, controls);
        if (!control.safe) {
            throw std::runtime_error(
                "no treatment candidate satisfies benefit, state, and RoH constraints");
        }

        HexAnchorAuditStore audit_store;
        const AnchorAuditDecision first_anchor = audit_store.append({
            1,
            "",
            "phoenix_h3_anchor_reference_001",
            0x8928308280fffffULL,
            4580,
            180'000,
            true,
            true
        });
        const AnchorAuditDecision second_anchor = audit_store.append({
            2,
            "phoenix_h3_anchor_reference_001",
            "phoenix_h3_anchor_reference_002",
            0x8928308280fffffULL,
            4725,
            240'000,
            true,
            true
        });
        const AnchorAuditDecision unsafe_anchor = audit_store.append({
            3,
            "phoenix_h3_anchor_reference_002",
            "phoenix_h3_anchor_reference_003",
            0x8928308280fffffULL,
            5180,
            340'000,
            true,
            false
        });

        const bool audit_safe = first_anchor.accepted &&
                                second_anchor.accepted &&
                                !unsafe_anchor.accepted &&
                                unsafe_anchor.invariant_holds;

        const double knowledge_factor = audit_safe
            ? 0.50 * control.knowledge_factor +
              0.50 * second_anchor.knowledge_factor
            : 0.0;
        const double eco_impact_value = audit_safe
            ? 0.50 * control.eco_impact_value +
              0.50 * second_anchor.eco_impact_value
            : 0.0;

        std::cout << std::fixed << std::setprecision(6)
                  << "safe_treatment_intensity="
                  << control.treatment_intensity << '\n'
                  << "hjb_local_value="
                  << control.hamilton_jacobi_bellman_value << '\n'
                  << "expected_next_invasive_abundance="
                  << control.expected_next_abundance << '\n'
                  << "diffusion_variance="
                  << control.diffusion_variance << '\n'
                  << "first_anchor_accepted="
                  << (first_anchor.accepted ? 1 : 0) << '\n'
                  << "second_anchor_accepted="
                  << (second_anchor.accepted ? 1 : 0) << '\n'
                  << "unsafe_anchor_accepted="
                  << (unsafe_anchor.accepted ? 1 : 0) << '\n'
                  << "anchor_invariant_holds="
                  << (unsafe_anchor.invariant_holds ? 1 : 0) << '\n'
                  << "accepted_anchor_count="
                  << audit_store.records().size() << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return audit_safe ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "stochastic invasive and anchor audit failed: "
                  << error.what() << '\n';
        return 1;
    }
}
