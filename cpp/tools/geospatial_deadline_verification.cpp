// File: cpp/tools/geospatial_deadline_verification.cpp
#include <iostream>
#include <stdexcept>

#include "../eco_restoration/geospatial_smt_deadline_verifier.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const FixedPointPolygon restoration_polygon{
            {1500, 1500}, {4200, 1500}, {4200, 3900}, {1500, 3900}
        };
        const FixedPointPolygon h3_cell_polygon{
            {3000, 1000}, {5000, 2000}, {5000, 4000},
            {3000, 5000}, {1000, 4000}, {1000, 2000}
        };

        const SmtResult intersection = fixed_point_polygon_cell_intersection(
            restoration_polygon, h3_cell_polygon);

        const ConsensusBounds bounds{
            12,
            3,
            8
        };
        const ConsensusState initial{
            ConsensusPhase::Waiting,
            0,
            0,
            1,
            0
        };
        const bool liveness = bounded_liveness_holds(initial, bounds);
        const auto initial_successors = consensus_successors(initial, bounds);

        const double knowledge_factor =
            (intersection == SmtResult::Sat ? 0.55 : 0.25) +
            (liveness ? 0.45 : 0.0);
        const double eco_impact_value =
            (intersection == SmtResult::Sat && liveness) ? 0.90 :
            (intersection == SmtResult::Sat ? 0.45 : 0.10);

        std::cout << "polygon_h3_intersection="
                  << (intersection == SmtResult::Sat ? "sat" : "unsat") << '\n'
                  << "initial_consensus_successor_count="
                  << initial_successors.size() << '\n'
                  << "bounded_liveness_holds=" << (liveness ? 1 : 0) << '\n'
                  << "clock_limit_ticks=" << bounds.clock_limit << '\n'
                  << "retry_limit=" << bounds.retry_limit << '\n'
                  << "queue_limit=" << bounds.queue_limit << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return intersection == SmtResult::Sat && liveness ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "geospatial deadline verification failed: "
                  << error.what() << '\n';
        return 1;
    }
}
