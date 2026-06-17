// include/EcoNetKarmaCore.hpp
#pragma once
#include <string>

namespace econet {

struct NodeKarmaResult {
    double kn;             // dimensionless node impact
    double ecoImpactScore; // 0-1 normalized eco-impact
};

NodeKarmaResult compute_kn_window(
    double cin,           // inflow concentration
    double cout,          // outflow concentration
    double cref,          // reference / corridor bound
    double flow,          // volumetric flow (m3/s)
    double duration_sec,  // window length in seconds
    double hazard_weight  // contaminant-specific weight
);

double normalize_ecoimpact(double kn);

} // namespace econet
