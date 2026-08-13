// File: cpp/tools/monotone_migration_private_score.cpp
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../eco_restoration/monotone_migration_and_private_score.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const CorridorPolicy baseline{
            {
                {1.0, 0.0, 10.0},
                {-1.0, 0.0, 0.0},
                {0.0, 1.0, 10.0},
                {0.0, -1.0, 0.0}
            },
            0.30
        };

        std::mt19937_64 generator(20260813ULL);
        const MigrationCase expansion = generate_half_space_expansion(
            baseline, generator, 2.0, 0.10);
        const MigrationCase shrink = shrink_half_space_counterexample(
            baseline, 1.0, 0.05);

        const std::vector<std::pair<double, double>> baseline_points{
            {0.0, 0.0},
            {5.0, 5.0},
            {10.0, 5.0},
            {7.5, 10.0}
        };
        const std::vector<double> baseline_risks{0.0, 0.12, 0.24, 0.30};

        const bool expansion_relaxes = is_relaxing_migration(expansion);
        const bool expansion_preserves = preserve_baseline_samples(
            expansion, baseline_points, baseline_risks);
        const bool shrink_relaxes = is_relaxing_migration(shrink);
        const bool shrink_preserves = preserve_baseline_samples(
            shrink, baseline_points, baseline_risks);

        const PrivateEcoWitness witness{
            {1250, 1180, 1315, 1288, 1199},
            0.84,
            0.18,
            0.96
        };
        const PublicEcoScoreJournal journal = build_private_score_journal(
            witness, "eco_witness_reference_20260813",
            "phoenix_corridor_policy_v1", 0.70, 1.00, 0.90);

        std::cout << std::fixed << std::setprecision(6)
                  << "expansion_is_relaxing=" << (expansion_relaxes ? 1 : 0) << '\n'
                  << "expansion_preserves_baseline_samples="
                  << (expansion_preserves ? 1 : 0) << '\n'
                  << "shrink_is_relaxing=" << (shrink_relaxes ? 1 : 0) << '\n'
                  << "shrink_preserves_baseline_samples="
                  << (shrink_preserves ? 1 : 0) << '\n'
                  << "private_score_accepted=" << (journal.accepted ? 1 : 0) << '\n'
                  << "journal_score=" << journal.score << '\n'
                  << "journal_risk_of_harm=" << journal.risk_of_harm << '\n'
                  << "knowledge_factor=" << journal.knowledge_factor << '\n'
                  << "eco_impact_value=" << journal.eco_impact_value << '\n';

        return expansion_relaxes && expansion_preserves &&
               !shrink_relaxes && !shrink_preserves && journal.accepted ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "monotone migration and private-score assessment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
