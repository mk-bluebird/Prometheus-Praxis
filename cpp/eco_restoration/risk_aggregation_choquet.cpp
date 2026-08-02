// File: cpp/eco_restoration/risk_aggregation_choquet.cpp

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <stdexcept>

// Component risk descriptor
struct ComponentRisk {
    std::string id;   // component identifier, e.g., "AdvectionKernel", "MARLController"
    double risk;      // RiskOfHarm coordinate for this component in [0,1]
};

// Fuzzy measure over subsets of components, represented via a map keyed by subset bitmask.
// For small numbers of components this is tractable and allows non-additive interactions.
struct FuzzyMeasure {
    // measure[mask] holds μ(S) where S is the subset corresponding to the bitmask "mask".
    // mask = 0 is implicitly μ(empty) = 0 and is not stored.
    std::vector<double> measure; // size = 1 << n_components

    double mu(unsigned int mask) const {
        if (mask >= measure.size()) {
            throw std::runtime_error("FuzzyMeasure mask out of range.");
        }
        return measure[mask];
    }
};

// Choquet integral aggregation for system risk
class ChoquetRiskAggregator {
public:
    // Aggregate sorted component risks using fuzzy measure μ.
    // Components must be sorted in non-decreasing order of risk.
    double aggregate(const std::vector<ComponentRisk>& comps,
                     const FuzzyMeasure& fm) const
    {
        if (comps.empty()) return 0.0;
        const std::size_t n = comps.size();

        // Sort by risk ascending; we assume the vector is already sorted by caller,
        // but enforce it defensively.
        std::vector<ComponentRisk> sorted = comps;
        std::sort(sorted.begin(), sorted.end(),
                  [](const ComponentRisk& a, const ComponentRisk& b) {
                      return a.risk < b.risk;
                  });

        // Compute cumulative set masks from highest-risk components down.
        // For Choquet integral: R_system = sum_i (r_(i) - r_(i-1)) * μ({C_(i),...,C_(n)})
        double prev = 0.0;
        double result = 0.0;

        for (std::size_t i = 0; i < n; ++i) {
            double current = sorted[i].risk;
            double delta = current - prev;
            prev = current;

            // Build mask for subset S_i = {C_(i), ..., C_(n-1)} in terms of original indices.
            unsigned int mask = 0;
            for (std::size_t j = i; j < n; ++j) {
                unsigned int idx = indexOf(comps, sorted[j].id);
                mask |= (1u << idx);
            }
            double mu_S = fm.mu(mask);
            result += delta * mu_S;
        }

        return result;
    }

private:
    static unsigned int indexOf(const std::vector<ComponentRisk>& v, const std::string& id) {
        for (unsigned int i = 0; i < v.size(); ++i) {
            if (v[i].id == id) return i;
        }
        throw std::runtime_error("Component id not found in original list.");
    }
};

// Example fuzzy measure construction for 2 components: Advection and MARL.
// We want non-additive risk when both fail simultaneously:
// μ({Advection}) = w_A, μ({MARL}) = w_M, μ({Advection, MARL}) = w_joint > w_A + w_M
FuzzyMeasure buildTwoComponentMeasure(double w_A, double w_M, double w_joint) {
    FuzzyMeasure fm;
    fm.measure.resize(4, 0.0); // 2^2 = 4 subsets
    // mask 0: empty set -> μ = 0
    fm.measure[0] = 0.0;
    // mask 1: {component 0} = Advection
    fm.measure[1] = w_A;
    // mask 2: {component 1} = MARL
    fm.measure[2] = w_M;
    // mask 3: {component 0,1} = Advection + MARL
    fm.measure[3] = w_joint;
    return fm;
}

// Example learning routine: fit w_joint from stress-test simulations.
// We assume we have simulated system risk residuals under different combinations:
// - Only Advection failed
// - Only MARL failed
// - Both failed simultaneously
struct StressTestSample {
    bool advection_failed;
    bool marl_failed;
    double system_risk_residual;
};

FuzzyMeasure learnTwoComponentMeasureFromStressTests(const std::vector<StressTestSample>& samples) {
    // Estimate weights by averaging observed system risk residuals for each configuration.
    double sum_only_A = 0.0; int count_only_A = 0;
    double sum_only_M = 0.0; int count_only_M = 0;
    double sum_both   = 0.0; int count_both   = 0;

    for (const auto& s : samples) {
        if (s.advection_failed && !s.marl_failed) {
            sum_only_A += s.system_risk_residual;
            ++count_only_A;
        } else if (!s.advection_failed && s.marl_failed) {
            sum_only_M += s.system_risk_residual;
            ++count_only_M;
        } else if (s.advection_failed && s.marl_failed) {
            sum_both += s.system_risk_residual;
            ++count_both;
        }
    }

    if (count_only_A == 0 || count_only_M == 0 || count_both == 0) {
        throw std::runtime_error("Insufficient stress-test samples to learn fuzzy measure.");
    }

    double w_A = sum_only_A / static_cast<double>(count_only_A);
    double w_M = sum_only_M / static_cast<double>(count_only_M);
    double w_joint = sum_both / static_cast<double>(count_both);

    // Enforce non-additive interaction: joint weight should be at least the max of individual
    // and can exceed their sum to reflect compounded risk.
    double min_joint = std::max(w_A + w_M, std::max(w_A, w_M));
    if (w_joint < min_joint) {
        w_joint = min_joint;
    }

    return buildTwoComponentMeasure(w_A, w_M, w_joint);
}

int main() {
    // Example: learn fuzzy measure from synthetic stress-test simulations.
    std::vector<StressTestSample> samples = {
        {true,  false, 0.35}, // Advection failure alone
        {true,  false, 0.32},
        {false, true, 0.30},  // MARL failure alone
        {false, true, 0.28},
        {true,  true, 0.70},  // Both fail simultaneously
        {true,  true, 0.65}
    };

    FuzzyMeasure fm = learnTwoComponentMeasureFromStressTests(samples);

    // Components and their current risk coordinates
    std::vector<ComponentRisk> components = {
        {"AdvectionKernel", 0.4},
        {"MARLController",  0.5}
    };

    ChoquetRiskAggregator aggregator;
    double system_risk = aggregator.aggregate(components, fm);

    std::cout << "Learned fuzzy measure weights:\n";
    std::cout << "mu({Advection}) = " << fm.mu(1u) << "\n";
    std::cout << "mu({MARL})      = " << fm.mu(2u) << "\n";
    std::cout << "mu({Adv,MARL})  = " << fm.mu(3u) << "\n";
    std::cout << "Aggregated system risk via Choquet integral = " << system_risk << "\n";

    return 0;
}
