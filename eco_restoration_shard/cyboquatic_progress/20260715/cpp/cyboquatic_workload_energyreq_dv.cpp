// filename: eco_restoration_shard/cyboquatic_progress/20260715/cpp/cyboquatic_workload_energyreq_dv.cpp
// purpose: Non-actuating cyboquatic workload model computing energy requirements (J)
//          and Lyapunov-consistent ΔVt for canal segments, designed for carbon-negative
//          optimization and SQLite telemetry logging.

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

// Simple workload record for a canal segment.
struct WorkloadSample {
    std::string segment_id;
    double flow_m3s;        // Q, cubic meters per second
    double head_loss_m;     // Δh, meters (hydraulic head loss)
    double density_kgm3;    // ρ, kg/m^3
    double g_ms2;           // g, m/s^2
    double vt_before;       // previous Lyapunov residual
    double vt_after;        // new residual
    double deltavt;         // vt_after - vt_before
    double energyreq_j;     // required energy in joules for this workload
    double k_factor;        // knowledge factor K in [0,1]
    double e_factor;        // eco-impact factor E in [0,1]
    double r_factor;        // residual risk factor R in [0,1]
};

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Compute energy requirement (J) for a hydraulic workload.
// Simplified formula: E = ρ * g * Q * Δh * t, with t = 1s for per-second workload.
static double compute_energyreq_j(double density_kgm3,
                                  double g_ms2,
                                  double flow_m3s,
                                  double head_loss_m) {
    if (density_kgm3 <= 0.0 || g_ms2 <= 0.0 || flow_m3s < 0.0 || head_loss_m < 0.0) {
        throw std::invalid_argument("Invalid physical parameters for energy computation.");
    }
    const double t_s = 1.0;
    return density_kgm3 * g_ms2 * flow_m3s * head_loss_m * t_s;
}

// Lyapunov residual Vt over normalized risk coordinates:
// Here we use two planes: hydraulics (r_h) and energy (r_e).
static double compute_vt(double r_h, double r_e) {
    const double w_h = 0.6;
    const double w_e = 0.4;
    return w_h * r_h * r_h + w_e * r_e * r_e;
}

// Normalize hydraulic and energy risk coordinates into [0,1].
static void normalize_risk(double flow_m3s,
                           double head_loss_m,
                           double energyreq_j,
                           double &r_h_out,
                           double &r_e_out) {
    // Corridor bounds (illustrative, to be tightened with field data):
    const double FLOW_SAFE_MAX = 5.0;        // m^3/s
    const double HEAD_SAFE_MAX = 2.0;        // m
    const double ENERGY_SAFE_MAX = 5e4;      // J per second

    double r_flow = (flow_m3s <= FLOW_SAFE_MAX)
                    ? (flow_m3s / FLOW_SAFE_MAX)
                    : 1.0;
    double r_head = (head_loss_m <= HEAD_SAFE_MAX)
                    ? (head_loss_m / HEAD_SAFE_MAX)
                    : 1.0;

    double r_h = 0.5 * (r_flow + r_head);
    double r_e = (energyreq_j <= ENERGY_SAFE_MAX)
                 ? (energyreq_j / ENERGY_SAFE_MAX)
                 : 1.0;

    r_h_out = clamp01(r_h);
    r_e_out = clamp01(r_e);
}

// Compute K,E,R scores from risk and ΔVt.
static void compute_ker(double vt,
                        double deltavt,
                        double &k_out,
                        double &e_out,
                        double &r_out) {
    double max_risk = clamp01(vt);  // vt already in [0,1]-scale if normalized appropriately

    double k = 0.95 - 0.3 * max_risk;
    if (deltavt > 0.0) {
        // Worsening residual penalizes knowledge.
        k -= 0.2;
    }
    if (k < 0.0) k = 0.0;

    double e = 0.95 - vt;
    if (deltavt > 0.0) {
        // Worsening residual penalizes eco-impact.
        e -= 0.15;
    }
    if (e < 0.0) e = 0.0;

    double r = vt + (deltavt > 0.0 ? deltavt : 0.0);
    if (r > 1.0) r = 1.0;

    k_out = clamp01(k);
    e_out = clamp01(e);
    r_out = clamp01(r);
}

// Construct a workload sample from raw physical parameters and previous vt.
static WorkloadSample make_workload(const std::string &segment_id,
                                    double flow_m3s,
                                    double head_loss_m,
                                    double density_kgm3,
                                    double g_ms2,
                                    double vt_before) {
    WorkloadSample sample{};
    sample.segment_id = segment_id;
    sample.flow_m3s = flow_m3s;
    sample.head_loss_m = head_loss_m;
    sample.density_kgm3 = density_kgm3;
    sample.g_ms2 = g_ms2;
    sample.vt_before = vt_before;

    sample.energyreq_j = compute_energyreq_j(density_kgm3, g_ms2, flow_m3s, head_loss_m);

    double r_h = 0.0, r_e = 0.0;
    normalize_risk(flow_m3s, head_loss_m, sample.energyreq_j, r_h, r_e);

    sample.vt_after = compute_vt(r_h, r_e);
    sample.deltavt = sample.vt_after - sample.vt_before;

    compute_ker(sample.vt_after, sample.deltavt, sample.k_factor, sample.e_factor, sample.r_factor);

    return sample;
}

// Example non-actuating main function: computes workloads for hypothetical Phoenix nodes.
int main() {
    try {
        std::vector<WorkloadSample> workloads;

        workloads.push_back(
            make_workload("PHX-CANAL-NODE-ENERGY-01",
                          3.0,          // flow m3/s
                          1.2,          // head loss m
                          1000.0,       // water density kg/m3
                          9.81,         // gravity m/s^2
                          0.25)         // vt_before
        );

        workloads.push_back(
            make_workload("PHX-CANAL-NODE-ENERGY-02",
                          4.5,
                          1.8,
                          1000.0,
                          9.81,
                          0.30)
        );

        for (const auto &w : workloads) {
            std::cout << "Segment: " << w.segment_id << "\n";
            std::cout << "  flow_m3s       = " << w.flow_m3s << "\n";
            std::cout << "  head_loss_m    = " << w.head_loss_m << "\n";
            std::cout << "  energyreq_j    = " << w.energyreq_j << "\n";
            std::cout << "  vt_before      = " << w.vt_before << "\n";
            std::cout << "  vt_after       = " << w.vt_after << "\n";
            std::cout << "  deltavt        = " << w.deltavt << "\n";
            std::cout << "  k_factor       = " << w.k_factor << "\n";
            std::cout << "  e_factor       = " << w.e_factor << "\n";
            std::cout << "  r_factor       = " << w.r_factor << "\n";
            std::cout << "-----------------------------\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
