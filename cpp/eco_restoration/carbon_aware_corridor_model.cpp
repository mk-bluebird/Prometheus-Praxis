// File: cpp/eco_restoration/carbon_aware_corridor_model.cpp
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

namespace eco {

enum class CarbonBand {
    GREEN_BAND,
    NEUTRAL,
    RED_BAND
};

struct KERProfile {
    double k; // knowledge factor in [0,1]
    double e; // eco-efficiency in [0,1]
    double r; // risk-of-harm in [0,1]

    KERProfile(double k_, double e_, double r_)
        : k(k_), e(e_), r(r_) {}
};

struct CarbonContext {
    double carbon_intensity; // CI
    double max_carbon;       // I_max
    double corridor_min;     // c_min for eco-significant workloads

    CarbonContext(double ci, double imax, double cmin)
        : carbon_intensity(ci), max_carbon(imax), corridor_min(cmin) {}
};

struct DriftParams {
    double gamma;        // coupling to KER scalar s
    double delta;        // coupling to carbon corridor c
    double delta_V_max;  // absolute cap on ΔV_t
    double alpha;        // eco penalty in E_eco
    double beta;         // Lyapunov weighting in ΔV_t = β E_eco

    DriftParams(double gamma_, double delta_,
                double delta_V_max_,
                double alpha_, double beta_)
        : gamma(gamma_), delta(delta_),
          delta_V_max(delta_V_max_),
          alpha(alpha_), beta(beta_) {}
};

struct WorkloadWindow {
    std::string id;
    KERProfile ker;
    bool is_research;
    double eco_energy;      // base energy E
    double eco_efficiency;  // eco_efficiency in [0,1]
    CarbonBand band;

    WorkloadWindow(const std::string& id_,
                   const KERProfile& ker_,
                   bool is_research_,
                   double eco_energy_,
                   double eco_eff_,
                   CarbonBand band_)
        : id(id_), ker(ker_), is_research(is_research_),
          eco_energy(eco_energy_), eco_efficiency(eco_eff_),
          band(band_) {}
};

// Lane-dependent KER thresholds
struct LaneThresholds {
    double s_min_non_research;
    double e_min_prod;
    double r_max_prod;
    double s_min_prod;

    LaneThresholds(double s_min_nr,
                   double e_min_p,
                   double r_max_p,
                   double s_min_p)
        : s_min_non_research(s_min_nr),
          e_min_prod(e_min_p),
          r_max_prod(r_max_p),
          s_min_prod(s_min_p) {}
};

double carbon_corridor(const CarbonContext& ctx) {
    if (ctx.max_carbon <= 0.0) {
        return 0.0;
    }
    double c = 1.0 - ctx.carbon_intensity / ctx.max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    if (c < ctx.corridor_min) {
        c = ctx.corridor_min;
    }
    return c;
}

double ker_scalar(const KERProfile& ker,
                  const LaneThresholds& lanes,
                  bool is_research) {
    double s = ker.k * ker.e - ker.r;
    if (!is_research && s < lanes.s_min_non_research) {
        s = lanes.s_min_non_research;
    }
    return s;
}

bool prod_lane_ok(const KERProfile& ker,
                  const LaneThresholds& lanes) {
    double s = ker.k * ker.e - ker.r;
    return (s > lanes.s_min_prod &&
            ker.e >= lanes.e_min_prod &&
            ker.r <= lanes.r_max_prod);
}

// Carbon band constraints linking band to KER thresholds and corridor
bool band_constraints_satisfied(const WorkloadWindow& w,
                                const CarbonContext& ctx,
                                const LaneThresholds& lanes) {
    double c = carbon_corridor(ctx);
    double s = ker_scalar(w.ker, lanes, w.is_research);

    switch (w.band) {
        case CarbonBand::GREEN_BAND:
            // Strongly eco-positive: high s, high e, low r, corridor near 1
            if (s <= 0.3) return false;
            if (w.ker.e < 0.8) return false;
            if (w.ker.r > 0.3) return false;
            if (c < 0.7) return false;
            return true;
        case CarbonBand::NEUTRAL:
            // Intermediate case: moderate KER and corridor
            if (s <= 0.1) return false;
            if (w.ker.e < 0.6) return false;
            if (w.ker.r > 0.6) return false;
            if (c < 0.3) return false;
            return true;
        case CarbonBand::RED_BAND:
            // Carbon-stressed: require strong KER to allow workloads at all
            if (s <= 0.3) return false;
            if (w.ker.e < 0.8) return false;
            if (w.ker.r > 0.3) return false;
            if (c < ctx.corridor_min) return false;
            return true;
    }
    return false;
}

// Eco energy term E_eco = E * (1 + alpha * (1 - eco_efficiency))
double eco_energy_term(const WorkloadWindow& w,
                       const DriftParams& params) {
    double eff = w.eco_efficiency;
    if (eff < 0.0) eff = 0.0;
    if (eff > 1.0) eff = 1.0;
    return w.eco_energy * (1.0 + params.alpha * (1.0 - eff));
}

// ΔV_t = β E_eco bounded by min(ΔV_max, γ s, δ c)
double drift_delta_V(const WorkloadWindow& w,
                     const CarbonContext& ctx,
                     const LaneThresholds& lanes,
                     const DriftParams& params) {
    double s = ker_scalar(w.ker, lanes, w.is_research);
    double c = carbon_corridor(ctx);
    double E_eco = eco_energy_term(w, params);

    double delta_V = params.beta * E_eco;
    double bound_ker = params.gamma * s;
    double bound_carbon = params.delta * c;

    double upper = params.delta_V_max;
    if (bound_ker < upper) upper = bound_ker;
    if (bound_carbon < upper) upper = bound_carbon;
    if (delta_V > upper) delta_V = upper;
    if (delta_V < 0.0) delta_V = 0.0;
    return delta_V;
}

// Actuation gate: decide whether physical control is allowed for a window.
bool actuation_allowed(const WorkloadWindow& w,
                       const CarbonContext& ctx,
                       const LaneThresholds& lanes,
                       const DriftParams& params,
                       double current_V_hex,
                       double V_hex_cap) {
    if (!band_constraints_satisfied(w, ctx, lanes)) {
        return false;
    }
    double deltaV = drift_delta_V(w, ctx, lanes, params);
    if (current_V_hex + deltaV > V_hex_cap) {
        return false;
    }
    return true;
}

// Example corridor evaluation for a sequence of workloads on a single hex.
void evaluate_hex_corridor(const std::vector<WorkloadWindow>& windows,
                           const CarbonContext& ctx,
                           const LaneThresholds& lanes,
                           const DriftParams& params,
                           double V0_hex,
                           double V_hex_cap) {
    double V_hex = V0_hex;
    for (const auto& w : windows) {
        bool ok = actuation_allowed(w, ctx, lanes, params, V_hex, V_hex_cap);
        double deltaV = drift_delta_V(w, ctx, lanes, params);
        std::cout << "Window " << w.id << " band=";
        switch (w.band) {
            case CarbonBand::GREEN_BAND: std::cout << "GREEN_BAND"; break;
            case CarbonBand::NEUTRAL: std::cout << "NEUTRAL"; break;
            case CarbonBand::RED_BAND: std::cout << "RED_BAND"; break;
        }
        std::cout << " KER=(" << w.ker.k << "," << w.ker.e << "," << w.ker.r << ")";
        std::cout << " ΔV=" << deltaV;
        std::cout << " -> ";
        if (ok) {
            V_hex += deltaV;
            std::cout << "ACTUATION_ALLOWED, V_hex=" << V_hex;
        } else {
            std::cout << "ACTUATION_BLOCKED, V_hex=" << V_hex;
        }
        std::cout << "\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    LaneThresholds lanes(/*s_min_non_research=*/0.05,
                         /*e_min_prod=*/0.7,
                         /*r_max_prod=*/0.5,
                         /*s_min_prod=*/0.2);

    CarbonContext ctx(/*carbon_intensity=*/0.4,
                      /*max_carbon=*/1.0,
                      /*corridor_min=*/0.2);

    DriftParams params(/*gamma=*/0.1,
                       /*delta=*/0.1,
                       /*delta_V_max=*/0.05,
                       /*alpha=*/0.5,
                       /*beta=*/0.01);

    std::vector<WorkloadWindow> windows;
    windows.emplace_back("w1",
                         KERProfile(0.9, 0.85, 0.25),
                         /*is_research=*/false,
                         /*eco_energy=*/1.2,
                         /*eco_eff_=*/0.9,
                         CarbonBand::GREEN_BAND);
    windows.emplace_back("w2",
                         KERProfile(0.7, 0.6, 0.4),
                         /*is_research=*/false,
                         /*eco_energy=*/1.0,
                         /*eco_eff_=*/0.8,
                         CarbonBand::NEUTRAL);
    windows.emplace_back("w3",
                         KERProfile(0.85, 0.82, 0.28),
                         /*is_research=*/false,
                         /*eco_energy=*/1.5,
                         /*eco_eff_=*/0.7,
                         CarbonBand::RED_BAND);

    double V0_hex = 0.0;
    double V_hex_cap = 0.5;

    evaluate_hex_corridor(windows, ctx, lanes, params, V0_hex, V_hex_cap);

    return 0;
}
