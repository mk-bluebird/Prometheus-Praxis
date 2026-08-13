// File: cpp/tools/private_heat_threat_assessment.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/private_heat_membership_threat_model.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const PrivateHeatProofPlan proof_plan{
            384,
            11,
            100,
            5400,
            180,
            16U * 1024U
        };
        const PrivateHeatStatement heat_statement =
            build_private_heat_statement(proof_plan, true, true, 0.96);

        const std::vector<ThreatObservation> observations{
            {ThreatSurface::SensorSpoofing, 0.08, 0.98, 1.00, 0.05},
            {ThreatSurface::ModelPoisoning, 0.06, 0.97, 1.00, 0.04},
            {ThreatSurface::PolicySubstitution, 0.01, 1.00, 1.00, 0.02},
            {ThreatSurface::DelayedActuation, 0.03, 0.99, 1.00, 0.08}
        };
        const ThreatAssessment threat =
            assess_ecological_system_threat(observations, 0.12);

        const bool corridor_safe = heat_statement.accepted && !threat.fail_closed;
        const double knowledge_factor = corridor_safe
            ? 0.50 * heat_statement.knowledge_factor +
              0.50 * threat.knowledge_factor
            : 0.0;
        const double eco_impact_value = corridor_safe
            ? 0.50 * heat_statement.eco_impact_value +
              0.50 * threat.eco_impact_value
            : 0.0;

        std::cout << std::fixed << std::setprecision(6)
                  << "private_heat_statement_accepted="
                  << (heat_statement.accepted ? 1 : 0) << '\n'
                  << "corridor_lookup_rows="
                  << heat_statement.membership_lookup_rows << '\n'
                  << "heat_range_lookup_rows="
                  << heat_statement.heat_range_lookup_rows << '\n'
                  << "threat_detectability=" << threat.detectability << '\n'
                  << "estimated_risk_of_harm="
                  << threat.estimated_risk_of_harm << '\n'
                  << "fail_closed=" << (threat.fail_closed ? 1 : 0) << '\n'
                  << "corridor_safe=" << (corridor_safe ? 1 : 0) << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return corridor_safe ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "private heat threat assessment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
