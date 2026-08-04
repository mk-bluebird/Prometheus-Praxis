// File: cpp/eco_restoration/fog_lyapunov_safety_certificate.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>

namespace prometheus_praxis {
namespace eco_restoration {

struct FOGRoutingState {
    double pfas_ugL;
    double bod_mgL;
    double temperature_c;
    double ker_e;
    double ker_r;
};

struct FOGRoutingAction {
    int action_id; // 0=SAFE,1=CAUTION,2=SHED,3=ROUTE_TO_TREATMENT,4=ROUTE_TO_RESTORATION
};

struct LyapunovWeights {
    double w_pfas;
    double w_bod;
    double w_temp;
    double w_ker_e;
    double w_ker_r;
};

double computeLyapunovCertificate(const FOGRoutingState& s,
                                  const LyapunovWeights& w,
                                  const FOGRoutingState& safe_ref) {
    double dpfas  = s.pfas_ugL      - safe_ref.pfas_ugL;
    double dbod   = s.bod_mgL       - safe_ref.bod_mgL;
    double dtemp  = s.temperature_c - safe_ref.temperature_c;
    double dker_e = s.ker_e         - safe_ref.ker_e;
    double dker_r = s.ker_r         - safe_ref.ker_r;

    double V = 0.0;
    V += w.w_pfas * dpfas  * dpfas;
    V += w.w_bod  * dbod   * dbod;
    V += w.w_temp * dtemp  * dtemp;
    V += w.w_ker_e* dker_e * dker_e;
    V += w.w_ker_r* dker_r * dker_r;
    return V;
}

FOGRoutingState gradientLyapunov(const FOGRoutingState& s,
                                 const LyapunovWeights& w,
                                 const FOGRoutingState& safe_ref) {
    FOGRoutingState g{};
    g.pfas_ugL      = 2.0 * w.w_pfas * (s.pfas_ugL      - safe_ref.pfas_ugL);
    g.bod_mgL       = 2.0 * w.w_bod  * (s.bod_mgL       - safe_ref.bod_mgL);
    g.temperature_c = 2.0 * w.w_temp * (s.temperature_c - safe_ref.temperature_c);
    g.ker_e         = 2.0 * w.w_ker_e* (s.ker_e         - safe_ref.ker_e);
    g.ker_r         = 2.0 * w.w_ker_r* (s.ker_r         - safe_ref.ker_r);
    return g;
}

FOGRoutingAction chooseSafeRoute(const FOGRoutingState& state,
                                 const LyapunovWeights& w,
                                 const FOGRoutingState& safe_ref,
                                 const FOGRoutingAction& candidate,
                                 const FOGRoutingAction& fallback) {
    bool is_restoration_route = (candidate.action_id == 4);
    if (is_restoration_route) {
        return candidate;
    }

    FOGRoutingState projected_state = state;

    if (candidate.action_id == 2) {
        projected_state.bod_mgL  += 0.5;
        projected_state.pfas_ugL += 0.02;
    } else if (candidate.action_id == 0) {
        projected_state.bod_mgL  -= 0.2;
        projected_state.pfas_ugL -= 0.01;
    } else if (candidate.action_id == 1) {
        projected_state.bod_mgL  += 0.1;
    }

    double V_current   = computeLyapunovCertificate(state, w, safe_ref);
    double V_projected = computeLyapunovCertificate(projected_state, w, safe_ref);

    if (V_projected <= V_current) {
        return candidate;
    }

    return fallback;
}

struct FOGState {
    double pfas_conc;
    double do_level;
    double flow_rate;
    double ker_e;
    double ker_r;
};

double computeLyapunovCertificate(const FOGState& s) {
    double r_pfas = s.pfas_conc;
    double r_do   = std::max(0.0, 6.0 - s.do_level);
    double r_flow = std::max(0.0, s.flow_rate - 1.0);
    double r_eco  = std::max(0.0, s.ker_e);
    double r_risk = std::max(0.0, s.ker_r);

    return r_pfas * r_pfas
         + r_do   * r_do
         + r_flow * r_flow
         + r_eco  * r_eco
         + r_risk * r_risk;
}

FOGState gradientLyapunov(const FOGState& s) {
    FOGState g{};
    double r_pfas = s.pfas_conc;
    double r_do   = std::max(0.0, 6.0 - s.do_level);
    double r_flow = std::max(0.0, s.flow_rate - 1.0);
    double r_eco  = std::max(0.0, s.ker_e);
    double r_risk = std::max(0.0, s.ker_r);

    g.pfas_conc = 2.0 * r_pfas;
    g.do_level  = (r_do   > 0.0) ? -2.0 * r_do   : 0.0;
    g.flow_rate = (r_flow > 0.0) ?  2.0 * r_flow : 0.0;
    g.ker_e     = (r_eco  > 0.0) ?  2.0 * r_eco  : 0.0;
    g.ker_r     = (r_risk > 0.0) ?  2.0 * r_risk : 0.0;

    return g;
}

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

        bool isPFASRestorationRoute = (route == "RESTORATION_CANAL");
        if (!isPFASRestorationRoute && deltaV >= 0.0) {
            continue;
        }

        double score = deltaV;
        if (score < bestScore) {
            bestScore = score;
            bestRoute = route;
        }
    }

    if (bestRoute.empty()) {
        return "RESTORATION_CANAL";
    }
    return bestRoute;
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main() {
    using namespace prometheus_praxis::eco_restoration;

    FOGRoutingState safe_ref;
    safe_ref.pfas_ugL      = 0.05;
    safe_ref.bod_mgL       = 5.0;
    safe_ref.temperature_c = 20.0;
    safe_ref.ker_e         = 0.8;
    safe_ref.ker_r         = 0.5;

    LyapunovWeights w;
    w.w_pfas  = 10.0;
    w.w_bod   = 5.0;
    w.w_temp  = 1.0;
    w.w_ker_e = 2.0;
    w.w_ker_r = 2.0;

    FOGRoutingState current;
    current.pfas_ugL      = 0.08;
    current.bod_mgL       = 6.0;
    current.temperature_c = 25.0;
    current.ker_e         = 0.9;
    current.ker_r         = 0.6;

    FOGRoutingAction candidate;
    candidate.action_id = 2;
    FOGRoutingAction fallback;
    fallback.action_id = 0;

    double V_current = computeLyapunovCertificate(current, w, safe_ref);
    FOGRoutingState grad = gradientLyapunov(current, w, safe_ref);
    FOGRoutingAction chosen = chooseSafeRoute(current, w, safe_ref, candidate, fallback);

    std::cout << "V(current)=" << V_current << std::endl;
    std::cout << "grad_pfas=" << grad.pfas_ugL
              << " grad_bod=" << grad.bod_mgL
              << " grad_temp=" << grad.temperature_c << std::endl;
    std::cout << "Chosen action_id=" << chosen.action_id << std::endl;

    FOGState s_cur;
    s_cur.pfas_conc = 0.08;
    s_cur.do_level  = 5.5;
    s_cur.flow_rate = 1.2;
    s_cur.ker_e     = 0.9;
    s_cur.ker_r     = 0.7;

    std::vector<std::string> routes = {"SAFE_CANAL", "BYPASS_CANAL", "RESTORATION_CANAL"};
    std::vector<FOGState> nextStates = {
        {0.06, 6.0, 1.0, 0.7, 0.5},
        {0.09, 5.0, 1.5, 0.9, 0.8},
        {0.04, 6.5, 0.8, 0.5, 0.3}
    };

    std::string safeRoute = chooseSafeRoute(s_cur, routes, nextStates);
    std::cout << "Chosen safe route=" << safeRoute << std::endl;

    return 0;
}
