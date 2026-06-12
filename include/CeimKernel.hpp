// filename: include/CeimKernel.hpp
// destination: eco_restoration_shard/include/CeimKernel.hpp
// Rust/ALN ecosystem note: this header defines a pure-math C++ CEIM kernel
// consistent with CEIM/CEIM-XJ design, without embedding policy logic.

#ifndef ECO_RESTORATION_SHARD_CEIMKERNEL_HPP
#define ECO_RESTORATION_SHARD_CEIMKERNEL_HPP

#include <string>
#include <vector>

namespace EcoNet {

struct CeimNodeSample {
    std::string node_id;
    std::string contaminant;
    double      cin;            // inflow concentration (canonical units)
    double      cout;           // outflow concentration (canonical units)
    double      flow;           // volumetric flow rate Q(t) approximated over window
    double      cref;           // reference concentration (EPA/ADEQ/WHO supremum)
    double      hazard_weight;  // x: hazard weighting factor
    std::string t0;             // window start (UTC ISO8601)
    std::string t1;             // window end   (UTC ISO8601)
};

struct CeimNodeResult {
    std::string node_id;
    std::string contaminant;
    double      kn;             // dimensionless CEIM Kn score
    double      ecoimpactscore; // normalized eco-impact score 0..1
};

class CeimKernel {
public:
    // Compute dimensionless Kn for a node sample.
    // This is a discrete approximation of:
    // Kn_x = x * (Cin - Cout) / Cref * Q * Δt
    // with Δt collapsed into the window definition.
    static double computeNodeKn(const CeimNodeSample& sample);

    // Compute eco-impact score (0..1) from Kn and hazard weight.
    // This function is monotone in Kn and bounded.
    static double computeEcoImpactScore(double kn, double ecoimpact_weight);

    // Optional helper to clamp scores into [0, 1].
    static double clamp01(double v);

private:
    CeimKernel() = delete;
};

} // namespace EcoNet

#endif // ECO_RESTORATION_SHARD_CEIMKERNEL_HPP
