// src/EcoNetKarmaCore.cpp
#include "EcoNetKarmaCore.hpp"
#include <algorithm>
#include <cmath>

namespace econet {

// Canonical CEIM node impact: Kn_x ~ (Cin - Cout) / Cref * Q * t, normalized.
NodeKarmaResult compute_kn_window(
    double cin,
    double cout,
    double cref,
    double flow,
    double duration_sec,
    double hazard_weight
) {
    // Guard against degenerate reference
    if (cref <= 0.0 || duration_sec <= 0.0 || flow < 0.0) {
        return {0.0, 0.0};
    }

    // Mass-load term M = (Cin - Cout) * Q * t
    const double delta_c = std::max(0.0, cin - cout);
    const double mass_load = delta_c * flow * duration_sec; // units from shard

    // Dimensionless node impact Kn_x = (Cin - Cout)/Cref * Q * t, scaled
    const double kn_raw = (delta_c / cref) * flow * duration_sec;

    // Apply hazard weight so high-risk contaminants dominate eco scoring
    const double kn_weighted = hazard_weight * kn_raw;

    NodeKarmaResult res;
    res.kn = kn_weighted;
    res.ecoImpactScore = normalize_ecoimpact(kn_weighted);
    return res;
}

// Map Kn into a 0-1 eco-impact score; larger positive Kn -> higher eco-impact.
double normalize_ecoimpact(double kn) {
    // Example: eco-impact saturates above a reference Kn_ref
    const double kn_ref = 1.0;
    if (kn <= 0.0) {
        return 0.0;
    }
    double e = kn / (kn + kn_ref); // monotone increasing, bounded < 1
    // Clamp to [0,1]
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;
    return e;
}

} // namespace econet
