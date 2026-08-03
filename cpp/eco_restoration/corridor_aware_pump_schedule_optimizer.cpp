// File: cpp/eco_restoration/corridor_aware_pump_schedule_optimizer.cpp
#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <cmath>

namespace eco {

struct CyboquaticWorkloadModel {
    // Simplified representation: per hour eco energy cost and KER/carbon context
    double eco_energy;      // base energy E
    double eco_efficiency;  // eco_efficiency in [0,1]
    double k;
    double e;
    double r;
    double carbon_intensity;
    double max_carbon;
};

struct PumpScheduleHour {
    int hour;           // 0..23
    int pump_on;        // binary decision variable 0/1
    double discharge;   // resulting discharge Q
    double delta_V;     // Lyapunov drift ΔV_t
};

struct CorridorParams {
    double gamma;
    double delta;
    double delta_V_max;
    double alpha;
    double beta;
    double s_min_non_research;
};

double carbon_corridor(double ci, double max_carbon, double c_min) {
    if (max_carbon <= 0.0) return 0.0;
    double c = 1.0 - ci / max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    if (c < c_min) c = c_min;
    return c;
}

double ker_scalar(double k, double e, double r, double s_min_non_research) {
    double s = k * e - r;
    if (s < s_min_non_research) s = s_min_non_research;
    return s;
}

// ΔV_t = β E_eco bounded by min(ΔV_max, γ s, δ c)
double delta_V_hour(const CyboquaticWorkloadModel& m,
                    const CorridorParams& p,
                    double c_min) {
    double eff = m.eco_efficiency;
    if (eff < 0.0) eff = 0.0;
    if (eff > 1.0) eff = 1.0;
    double E_eco = m.eco_energy * (1.0 + p.alpha * (1.0 - eff));

    double s = ker_scalar(m.k, m.e, m.r, p.s_min_non_research);
    double c = carbon_corridor(m.carbon_intensity, m.max_carbon, c_min);

    double dV = p.beta * E_eco;
    double bound_ker = p.gamma * s;
    double bound_carbon = p.delta * c;
    double upper = p.delta_V_max;
    if (bound_ker < upper) upper = bound_ker;
    if (bound_carbon < upper) upper = bound_carbon;
    if (dV > upper) dV = upper;
    if (dV < 0.0) dV = 0.0;
    return dV;
}

// Simple MILP-style heuristic: choose pump_on per hour to minimize energy
// while keeping cumulative ΔV within corridor caps.
std::vector<PumpScheduleHour> optimize_pump_schedule(
        const std::vector<CyboquaticWorkloadModel>& models,
        const CorridorParams& params,
        double c_min,
        double V_hex_cap) {

    std::vector<PumpScheduleHour> schedule;
    schedule.reserve(models.size());

    double cumulative_V = 0.0;

    for (std::size_t h = 0; h < models.size(); ++h) {
        const auto& m = models[h];
        PumpScheduleHour ph{};
        ph.hour = static_cast<int>(h);

        // Two candidate decisions: pump off/on
        double dV_on = delta_V_hour(m, params, c_min);
        double dV_off = 0.0;

        // Corridor-aware choice: prefer off if cumulative_V is near cap
        bool choose_on = false;
        if (cumulative_V + dV_on <= V_hex_cap) {
            // Simple objective: minimize total energy; here eco_energy ~ energy
            // choose on only if needed for corridor (e.g., high pollution/heat, encoded via k,e,r).
            double s = m.k * m.e - m.r;
            if (s < 0.2) {
                // low KER, avoid pumping
                choose_on = false;
            } else {
                choose_on = true;
            }
        }

        if (choose_on) {
            ph.pump_on = 1;
            ph.discharge = 1.0; // normalized discharge
            ph.delta_V = dV_on;
            cumulative_V += dV_on;
        } else {
            ph.pump_on = 0;
            ph.discharge = 0.0;
            ph.delta_V = dV_off;
        }

        schedule.push_back(ph);
    }

    return schedule;
}

void print_schedule(const std::vector<PumpScheduleHour>& schedule) {
    std::cout << "Corridor-aware 24h pump schedule:\n";
    for (const auto& h : schedule) {
        std::cout << "Hour " << h.hour
                  << " pump_on=" << h.pump_on
                  << " discharge=" << h.discharge
                  << " ΔV=" << h.delta_V << "\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example 24-hour CyboquaticWorkloadModel sequence
    std::vector<CyboquaticWorkloadModel> models;
    for (int h = 0; h < 24; ++h) {
        double eco_energy = 1.0 + 0.1 * std::sin(h * 3.141592653589793 / 12.0);
        double eco_eff = 0.85;
        double k = 0.8;
        double e = 0.8;
        double r = 0.3;
        double ci = 0.4 + 0.05 * std::cos(h * 3.141592653589793 / 12.0);
        double imax = 1.0;
        models.push_back({eco_energy, eco_eff, k, e, r, ci, imax});
    }

    CorridorParams params{};
    params.gamma = 0.1;
    params.delta = 0.1;
    params.delta_V_max = 0.05;
    params.alpha = 0.5;
    params.beta = 0.01;
    params.s_min_non_research = 0.05;

    double c_min = 0.2;
    double V_hex_cap = 0.5;

    auto schedule = optimize_pump_schedule(models, params, c_min, V_hex_cap);
    print_schedule(schedule);

    return 0;
}
