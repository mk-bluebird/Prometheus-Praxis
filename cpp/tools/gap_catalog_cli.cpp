// File: cpp/tools/gap_catalog_cli.cpp
#include "reliability_gap_catalog.hpp"

#include <iostream>
#include <iomanip>

using namespace praxis::governance;

static void print_gap(const GovernanceGap& g) {
    std::cout << "=== Governance Gap: " << g.clause_id
              << " (" << g.hex_stamp << ") ===\n";
    std::cout << "Kind: "
              << (g.kind == GapKind::MISSING_PRECONDITION ? "Missing Precondition"
                                                          : "Bypass Path")
              << "\n\n";

    std::cout << "Expected Enforcement Mechanism:\n"
              << "  " << g.expected_enforcement_mechanism << "\n\n";

    std::cout << "Observed Behavior:\n"
              << "  " << g.observed_behavior << "\n\n";

    std::cout << "EcoImpactScore:\n"
              << "  knowledge_factor = " << std::fixed << std::setprecision(2)
              << g.score.knowledge_factor
              << ", eco_impact_value = " << g.score.eco_impact_value
              << "\n\n";

    std::cout << "Remediation Steps:\n";
    for (std::size_t i = 0; i < g.remediation_steps.size(); ++i) {
        std::cout << "  " << (i + 1) << ". "
                  << g.remediation_steps[i].description << "\n";
    }
    std::cout << "\n";
}

int main() {
    auto gaps = build_gap_catalog();

    std::cout << "# Gap Catalog for 0x20260729PHXCHATLABORPSYCHCONTINUITY\n\n";
    for (const auto& g : gaps) {
        print_gap(g);
    }

    double sum_kf = 0.0;
    double sum_ev = 0.0;
    for (const auto& g : gaps) {
        sum_kf += g.score.knowledge_factor;
        sum_ev += g.score.eco_impact_value;
    }
    double avg_kf = sum_kf / static_cast<double>(gaps.size());
    double avg_ev = sum_ev / static_cast<double>(gaps.size());

    std::cout << "=== Catalog Summary ===\n";
    std::cout << "Average knowledge_factor: " << std::fixed
              << std::setprecision(2) << avg_kf << "\n";
    std::cout << "Average eco_impact_value: " << std::fixed
              << std::setprecision(2) << avg_ev << "\n";

    return 0;
}
