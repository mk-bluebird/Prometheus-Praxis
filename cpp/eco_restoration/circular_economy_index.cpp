// File: cpp/eco_restoration/circular_economy_index.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <cmath>

struct NodeLifecycleData {
    double mass_recycled_kg;        // mass of materials recycled at end-of-life
    double reusable_energy_kwh;     // energy recoverable (e.g., via reuse, repowering)
    double total_mass_produced_kg;  // total hardware mass produced per node
};

struct CircularEconomyThresholds {
    double min_circ_index_positive;     // minimum I_circ for positive EcoImpact contribution
    double min_circ_index_high_impact;  // threshold for strong circularity
};

struct EcoImpactAssessment {
    double I_circ;
    double eco_impact_score;        // normalized [0,1]
    bool contributes_positive;      // true if I_circ >= min_circ_index_positive
};

class CircularEconomyIndexCalculator {
public:
    EcoImpactAssessment assess(const NodeLifecycleData& data,
                               const CircularEconomyThresholds& thresholds) const
    {
        if (data.total_mass_produced_kg <= 0.0) {
            throw std::runtime_error("Invalid total mass for circular economy index.");
        }

        // Convert reusable energy to an equivalent mass term via a simple factor
        // (e.g., kwh_to_mass_factor_kg_per_kwh derived from local LCA studies).
        // For demonstration we use a small factor to avoid over-weighting energy.
        const double kwh_to_mass_factor = 0.01; // kg per kWh equivalent

        double equivalent_reusable_mass = data.reusable_energy_kwh * kwh_to_mass_factor;

        double I_circ = (data.mass_recycled_kg + equivalent_reusable_mass) /
                        data.total_mass_produced_kg;

        EcoImpactAssessment a;
        a.I_circ = I_circ;
        a.contributes_positive = (I_circ >= thresholds.min_circ_index_positive);

        // Map I_circ to eco_impact_score: 0 below positive threshold, rising to 1 at high impact.
        if (I_circ <= thresholds.min_circ_index_positive) {
            a.eco_impact_score = 0.0;
        } else if (I_circ >= thresholds.min_circ_index_high_impact) {
            a.eco_impact_score = 1.0;
        } else {
            double range = thresholds.min_circ_index_high_impact - thresholds.min_circ_index_positive;
            a.eco_impact_score = (I_circ - thresholds.min_circ_index_positive) / range;
        }

        return a;
    }
};

struct EnergyNeutralityLCA {
    double manufacture_energy_kwh;     // energy used to produce the node
    double operation_energy_kwh;       // expected lifetime operation energy
    double reusable_energy_kwh;        // energy recovered via circular practices
    double renewable_supply_kwh;       // renewable energy available for this node over lifetime
};

bool check_energy_neutral_or_renew(const EnergyNeutralityLCA& lca,
                                   const EcoImpactAssessment& eco,
                                   double max_nonrenewable_fraction,
                                   std::string& reason)
{
    // Life-cycle inequality:
    // manufacture + operation - reusable <= renewable + eco_bonus
    double eco_bonus_kwh = eco.eco_impact_score * 0.1 * lca.renewable_supply_kwh;

    double net_nonrenewable = lca.manufacture_energy_kwh + lca.operation_energy_kwh
                              - lca.reusable_energy_kwh
                              - lca.renewable_supply_kwh
                              - eco_bonus_kwh;

    if (net_nonrenewable <= 0.0) {
        reason.clear();
        return true;
    }

    double fraction_nonrenewable = net_nonrenewable /
                                   (lca.manufacture_energy_kwh + lca.operation_energy_kwh);

    if (fraction_nonrenewable <= max_nonrenewable_fraction) {
        reason.clear();
        return true;
    }

    reason = "Node life-cycle exceeds allowed non-renewable energy fraction.";
    return false;
}

int main() {
    // Example lifecycle data for a Phoenix sensor node
    NodeLifecycleData node{
        0.4,  // mass_recycled_kg
        5.0,  // reusable_energy_kwh (e.g., repurposed battery/solar components)
        0.8   // total_mass_produced_kg
    };

    CircularEconomyThresholds thresholds{
        0.5,  // min I_circ to be considered positive (at least half of mass/energy circular)
        0.9   // high-impact circularity threshold
    };

    CircularEconomyIndexCalculator calculator;
    EcoImpactAssessment assessment = calculator.assess(node, thresholds);

    std::cout << "Circular economy index I_circ = " << assessment.I_circ << "\n";
    std::cout << "EcoImpact score from circularity = " << assessment.eco_impact_score << "\n";
    std::cout << "Contributes positively to EcoImpact: "
              << (assessment.contributes_positive ? "true" : "false") << "\n";

    // Integrate into energy_neutral_or_renew gate via life-cycle assessment
    EnergyNeutralityLCA lca{
        50.0,  // manufacture_energy_kwh
        200.0, // operation_energy_kwh over lifetime
        node.reusable_energy_kwh,
        220.0  // renewable_supply_kwh over lifetime
    };

    std::string reason;
    bool energy_neutral = check_energy_neutral_or_renew(lca, assessment,
                                                        0.05, // max 5% non-renewable fraction
                                                        reason);

    std::cout << "energy_neutral_or_renew: " << (energy_neutral ? "true" : "false") << "\n";
    if (!energy_neutral) {
        std::cout << "Reason: " << reason << "\n";
    }

    return 0;
}
