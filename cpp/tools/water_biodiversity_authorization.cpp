// File: cpp/tools/water_biodiversity_authorization.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "../eco_restoration/water_biodiversity_and_actuation_authorization.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const WaterAllocation water{
            720'000,
            1'000'000,
            180'000
        };
        const BiodiversityIndex biodiversity{
            810'000,
            750'000
        };
        const CrossShardDecision decision =
            evaluate_water_biodiversity(water, biodiversity);

        const bool invariant_holds = required_cross_shard_unsat(decision);
        if (!invariant_holds) {
            throw std::runtime_error("cross-shard biodiversity invariant violated");
        }

        ProofCheckedDispatcher dispatcher("eco_actuation_policy_v1");
        const AuthorizationEvidence evidence{
            "canopy_irrigation_survey",
            "eco_actuation_policy_v1",
            1'760'003'000,
            1'760'006'600,
            1,
            180'000,
            true
        };
        const bool authorization_accepted =
            dispatcher.accept(evidence, 1'760'004'200);

        const double authorization_knowledge = authorization_accepted
            ? 0.95 * decision.knowledge_factor
            : 0.0;
        const double eco_impact_value = authorization_accepted && decision.allow
            ? decision.eco_impact_value
            : 0.0;

        std::cout << std::fixed << std::setprecision(6)
                  << "water_compliant=" << (decision.water_compliant ? 1 : 0) << '\n'
                  << "biodiversity_compliant="
                  << (decision.biodiversity_compliant ? 1 : 0) << '\n'
                  << "cross_shard_invariant_holds="
                  << (invariant_holds ? 1 : 0) << '\n'
                  << "authorization_accepted="
                  << (authorization_accepted ? 1 : 0) << '\n'
                  << "approved_action_identifier="
                  << (authorization_accepted
                      ? dispatcher.latest_approved_actuation().action_identifier
                      : "none") << '\n'
                  << "knowledge_factor=" << authorization_knowledge << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return decision.allow && authorization_accepted ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "water biodiversity authorization failed: "
                  << error.what() << '\n';
        return 1;
    }
}
