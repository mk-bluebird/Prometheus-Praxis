// filename: ecorestorationshard/cyboquatic_progress/20260720/cpp/blastradius_model_20260720.cpp
// destination: ecorestorationshard/cyboquatic_progress/20260720/cpp/blastradius_model_20260720.cpp
// domain: g (blast-radius surcharge envelopes).[file:2]

#include <cmath>
#include <iostream>

struct BlastRadiusInput {
    double surcharge_level_m;   // meters above safe freeboard.
    double design_flow_m3s;     // canal design flow (m^3/s).
};

struct BlastRadiusOutput {
    double breach_prob;         // probability in [0,1].
    double radius_m;            // blast-impact radius (m).
};

static BlastRadiusOutput compute_blast_radius(const BlastRadiusInput &in) {
    const double alpha = 2.5;   // sensitivity to surcharge.
    const double beta  = 0.01;  // flow coupling.[file:13]

    double surcharge = std::max(0.0, in.surcharge_level_m);
    double flow_term = std::max(0.0, in.design_flow_m3s);

    // breach probability: logistic in surcharge and flow (non-actuating diagnostic).[file:13]
    double x = alpha * surcharge + beta * flow_term;
    double breach_prob = 1.0 / (1.0 + std::exp(-x));

    // radius scales sublinearly with surcharge, capped to avoid unbounded blast radius.[file:13]
    double radius_m = 10.0 + 50.0 * std::tanh(surcharge);

    return BlastRadiusOutput{breach_prob, radius_m};
}

int main() {
    BlastRadiusInput in{0.30, 12.5};
    BlastRadiusOutput out = compute_blast_radius(in);

    std::cout << "breach_prob=" << out.breach_prob
              << " radius_m="   << out.radius_m << "\n";

    return 0;
}
