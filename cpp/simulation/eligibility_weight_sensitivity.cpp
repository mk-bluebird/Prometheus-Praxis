// File: cpp/simulation/eligibility_weight_sensitivity.cpp

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

struct ComponentRisk {
    std::string id;
    double risk_coordinate; // e.g., RiskOfHarm for this component in [0,1]
    double weight;          // current weight w_j in system aggregation
    bool   eligible;        // current eligibility status for this component
};

struct SystemRiskResult {
    double system_risk;
};

class LinearRiskAggregator {
public:
    // Simple linear aggregation: R_system = sum_j w_j * r_j
    SystemRiskResult aggregate(const std::vector<ComponentRisk>& comps) const {
        double R = 0.0;
        for (const auto& c : comps) {
            R += c.weight * c.risk_coordinate;
        }
        return {R};
    }

    // Gradient dR_system / dw_j = r_j
    double gradient_wrt_weight(const ComponentRisk& comp) const {
        return comp.risk_coordinate;
    }

    // Compute minimal worst-case weight perturbation that flips a component from Eligible to NotEligible.
    // We assume a component is considered NotEligible when its contribution pushes R_system above a threshold.
    double minimal_weight_perturbation_to_flip(const std::vector<ComponentRisk>& comps,
                                               const std::string& target_id,
                                               double risk_threshold,
                                               bool& flip_direction_increase) const
    {
        SystemRiskResult base = aggregate(comps);
        double R_base = base.system_risk;

        const ComponentRisk* target = nullptr;
        for (const auto& c : comps) {
            if (c.id == target_id) {
                target = &c;
                break;
            }
        }
        if (!target) {
            throw std::runtime_error("Target component not found.");
        }

        // Worst-case direction: increase weight if r_j > 0, decrease if r_j < 0.
        double rj = target->risk_coordinate;
        if (rj == 0.0) {
            flip_direction_increase = true;
            return std::numeric_limits<double>::infinity();
        }

        // We want |R_new - R_base| >= |risk_threshold - R_base| in the direction that crosses the threshold.
        // R_new = R_base + delta_w * r_j.
        double delta_needed = risk_threshold - R_base;

        // For flipping from Eligible (R_base <= risk_threshold) to NotEligible (R_new > risk_threshold),
        // we consider increasing R_system.
        flip_direction_increase = true;
        if (delta_needed <= 0.0) {
            // Already above threshold; no perturbation needed to remain NotEligible.
            return 0.0;
        }

        double delta_w = delta_needed / rj;
        if (delta_w < 0.0) {
            // If r_j < 0, increasing weight decreases risk; worst-case is decreasing weight instead.
            flip_direction_increase = false;
            delta_w = delta_needed / (-rj);
        }

        return std::abs(delta_w);
    }
};

int main() {
    // Example component corridor configuration
    std::vector<ComponentRisk> components = {
        {"AdvectionKernel", 0.30, 0.20, true},
        {"MARLController",  0.35, 0.25, true},
        {"MistingSystem",   0.25, 0.30, true},
        {"DataPipeline",    0.20, 0.25, true}
    };

    LinearRiskAggregator agg;
    SystemRiskResult base = agg.aggregate(components);
    double R_threshold = 0.35; // threshold between Eligible and NotEligible

    std::cout << "Base system risk: " << base.system_risk << "\n";
    std::cout << "Eligibility threshold: " << R_threshold << "\n";

    bool dir_increase = true;
    double min_delta_w = agg.minimal_weight_perturbation_to_flip(
        components, "MARLController", R_threshold, dir_increase
    );

    std::cout << "Minimal worst-case weight perturbation on MARLController to flip eligibility: "
              << min_delta_w << " (direction: "
              << (dir_increase ? "increase weight" : "decrease weight") << ")\n";

    // This sensitivity can be used to detect over-parameterised corridors:
    // if tiny changes in weights for certain components flip eligibility often,
    // those corridors are overly sensitive and need simplification or stronger evidence.
    return 0;
}
