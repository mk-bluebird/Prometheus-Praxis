// File: cpp/eco_restoration/fog_lyapunov_safety_certificate.cpp

#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <sqlite3.h>

// State vector for FOG routing safety
struct FOGState {
    double pfas_conc;       // PFAS concentration/risk coordinate
    double do_level;        // dissolved oxygen
    double flow_rate;       // flow
    double ker_e;           // eco-impact score
    double ker_r;           // risk-of-harm score
};

// Lyapunov safety certificate V(s) ~ sum of squared risk coordinates
double computeLyapunovCertificate(const FOGState& s) {
    // Example Lyapunov candidate; in practice coefficients come from SOS programming.
    double r_pfas = s.pfas_conc;
    double r_do   = std::max(0.0, 6.0 - s.do_level); // deficit from safe DO
    double r_flow = std::max(0.0, s.flow_rate - 1.0);
    double r_eco  = std::max(0.0, s.ker_e);
    double r_risk = std::max(0.0, s.ker_r);

    return r_pfas * r_pfas
         + r_do   * r_do
         + r_flow * r_flow
         + r_eco  * r_eco
         + r_risk * r_risk;
}

// Gradient of V(s) with respect to state, used as soft constraint
FOGState gradientLyapunov(const FOGState& s) {
    FOGState g{};
    double r_pfas = s.pfas_conc;
    double r_do   = std::max(0.0, 6.0 - s.do_level);
    double r_flow = std::max(0.0, s.flow_rate - 1.0);
    double r_eco  = std::max(0.0, s.ker_e);
    double r_risk = std::max(0.0, s.ker_r);

    g.pfas_conc = 2.0 * r_pfas;
    g.do_level  = (r_do > 0.0) ? -2.0 * r_do : 0.0;
    g.flow_rate = (r_flow > 0.0) ?  2.0 * r_flow : 0.0;
    g.ker_e     = (r_eco  > 0.0) ?  2.0 * r_eco  : 0.0;
    g.ker_r     = (r_risk > 0.0) ?  2.0 * r_risk : 0.0;

    return g;
}

// Safety-aware routing decision using V(s) and its gradient.
// Non-PFAS routes must satisfy decrease in V; PFAS->restoration may allow non-decrease.
std::string chooseSafeRoute(const FOGState& current,
                            const std::vector<std::string>& candidateRoutes,
                            const std::vector<FOGState>& predictedNextStates) {
    if (candidateRoutes.size() != predictedNextStates.size()) {
        throw std::runtime_error("Route and state vectors length mismatch");
    }

    double V_current = computeLyapunovCertificate(current);
    std::string bestRoute;
    double bestScore = std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < candidateRoutes.size(); ++i) {
        const auto& route = candidateRoutes[i];
        const auto& next  = predictedNextStates[i];

        double V_next = computeLyapunovCertificate(next);
        double deltaV = V_next - V_current;

        // Hard constraint: non-PFAS routes must strictly decrease V
        bool isPFASRestorationRoute = (route == "RESTORATION_CANAL");

        if (!isPFASRestorationRoute && deltaV >= 0.0) {
            continue; // reject unsafe route
        }

        // Soft scoring: prefer routes with largest negative deltaV
        double score = deltaV;
        if (score < bestScore) {
            bestScore = score;
            bestRoute = route;
        }
    }

    if (bestRoute.empty()) {
        // Fallback: route PFAS to restoration canal
        return "RESTORATION_CANAL";
    }
    return bestRoute;
}
