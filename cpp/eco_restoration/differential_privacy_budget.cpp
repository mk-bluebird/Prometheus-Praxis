// File: cpp/eco_restoration/differential_privacy_budget.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>

struct PrivacyMechanismConfig {
    // Total daily privacy budget for the pipeline (epsilon_total).
    double epsilon_total_per_day;
    // Max per-query epsilon to limit individual query impact.
    double epsilon_per_query_max;
};

struct PrivacyUsageState {
    double epsilon_used_today;
    int    queries_answered_today;
};

struct SovereigntyAgreement {
    // Maximum legally/ethically permitted reidentification risk for tribal and census-tract data.
    // This is expressed as an equivalent epsilon bound derived from agreements and statutes.
    double max_reidentification_risk_percent; // e.g., 2% as in HIPAA-like guidance [287]
    double max_epsilon_per_person_per_day;    // per-person daily epsilon upper bound
};

class DifferentialPrivacyBudgetManager {
public:
    DifferentialPrivacyBudgetManager(const PrivacyMechanismConfig& cfg,
                                     const SovereigntyAgreement& agr)
        : cfg_(cfg), agreement_(agr)
    {
        if (cfg_.epsilon_total_per_day <= 0.0 || cfg_.epsilon_per_query_max <= 0.0) {
            throw std::runtime_error("Invalid privacy mechanism configuration.");
        }
        if (agreement_.max_epsilon_per_person_per_day <= 0.0) {
            throw std::runtime_error("Invalid sovereignty agreement epsilon bound.");
        }
    }

    // Decide per-query epsilon, bounded by both mechanism config and sovereignty agreement.
    double allocate_epsilon_for_query(const PrivacyUsageState& state,
                                      int num_persons_affected) const
    {
        // Base per-query epsilon capped by mechanism configuration.
        double base_eps = cfg_.epsilon_per_query_max;

        // Sovereignty constraint: per-person per-day epsilon must not exceed agreement bound.
        // If query affects num_persons_affected individuals, the effective per-person epsilon is
        // base_eps / num_persons_affected.
        double per_person_eps = base_eps / std::max(1, num_persons_affected);

        if (per_person_eps > agreement_.max_epsilon_per_person_per_day) {
            // Scale down epsilon to satisfy tribal/census-tract privacy agreements.
            double scale = agreement_.max_epsilon_per_person_per_day / per_person_eps;
            base_eps *= scale;
        }

        // Remaining budget constraint: cannot exceed epsilon_total_per_day.
        double remaining = cfg_.epsilon_total_per_day - state.epsilon_used_today;
        if (remaining <= 0.0) {
            // Budget exhausted: sovereignty_compliant must be false upstream.
            return 0.0;
        }

        if (base_eps > remaining) {
            base_eps = remaining;
        }

        return base_eps;
    }

    // Update usage state after a query; return whether sovereignty_compliant remains true.
    bool apply_query(PrivacyUsageState& state,
                     int num_persons_affected,
                     bool& sovereignty_compliant) const
    {
        double eps = allocate_epsilon_for_query(state, num_persons_affected);
        if (eps <= 0.0) {
            sovereignty_compliant = false;
            return false;
        }

        state.epsilon_used_today += eps;
        state.queries_answered_today += 1;

        if (state.epsilon_used_today >= cfg_.epsilon_total_per_day) {
            sovereignty_compliant = false;
        } else {
            sovereignty_compliant = true;
        }

        return true;
    }

private:
    PrivacyMechanismConfig cfg_;
    SovereigntyAgreement   agreement_;
};

int main() {
    // Example configuration:
    // Total daily epsilon budget per tract: epsilon_total_per_day ~ 2.0 (similar to real-world DP uses) [280][290]
    // Max per-query epsilon: 0.4, so multiple queries can be answered with controlled noise.
    PrivacyMechanismConfig cfg{
        2.0,  // epsilon_total_per_day
        0.4   // epsilon_per_query_max
    };

    // Tribal data sovereignty agreements constrain per-person epsilon and reidentification risk
    // (e.g., <= 2% risk mapped to epsilon <= 0.08 as in legal analyses [287][278][281][285][288][291]).
    SovereigntyAgreement agr{
        2.0,   // max_reidentification_risk_percent
        0.08   // max_epsilon_per_person_per_day
    };

    DifferentialPrivacyBudgetManager manager(cfg, agr);
    PrivacyUsageState state{0.0, 0};

    bool sovereignty_ok = true;

    // Simulate a day of heat-monitoring queries over Phoenix census tracts.
    // Each query aggregates MRT over a tract, affecting ~1000 persons.
    const int num_queries = 8;
    const int persons_per_query = 1000;

    for (int q = 0; q < num_queries; ++q) {
        bool applied = manager.apply_query(state, persons_per_query, sovereignty_ok);
        std::cout << "Query " << (q + 1)
                  << ": epsilon_used_today=" << state.epsilon_used_today
                  << ", sovereignty_compliant=" << (sovereignty_ok ? "true" : "false")
                  << "\n";

        if (!applied || !sovereignty_ok) {
            std::cout << "Privacy budget exhausted or sovereignty agreement violated; "
                         "no further queries allowed today.\n";
            break;
        }
    }

    return 0;
}
