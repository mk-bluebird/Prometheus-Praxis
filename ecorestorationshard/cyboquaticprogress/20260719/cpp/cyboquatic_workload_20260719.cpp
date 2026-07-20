// filename: ecorestorationshard/cyboquaticprogress/20260719/cpp/cyboquatic_workload_20260719.cpp
// destination: ecorestorationshard/cyboquaticprogress/20260719/cpp/cyboquatic_workload_20260719.cpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
// domain: Cyboquatic workload (energyreqJ, ΔVt) non-actuating model.
// This C++ module computes energyreqJ and Lyapunov residual ΔVt for a single
// cyboquatic workload window, enforcing corridor-style invariants but never
// touching physical actuators. It is intended to be invoked offline and its
// outputs written into dbcyboquaticdailyprogress.sqlite via Java/Kotlin tools. [file:2][file:32]

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

struct WorkloadPlanes {
    double r_energy;
    double r_carbon;
    double r_hydraulics;
    double r_materials;
    double r_dataquality;
};

struct KerTriad {
    double k;
    double e;
    double r;
};

struct WorkloadResult {
    double energyreq_j;
    double energy_j_baseline;
    double delta_energy_j;
    double vt_prev;
    double vt_curr;
    double delta_vt;
    WorkloadPlanes planes;
    KerTriad ker;
};

static void validate_plane(double v, const char* name) {
    if (v < 0.0 || v > 1.0) {
        throw std::runtime_error(std::string("Plane out of [0,1]: ") + name);
    }
}

static double lyapunov_g(double r, double alpha, double beta, double p) {
    // Strictly convex, super-quadratic penalty g(r) = alpha * r^2 + beta * r^p, p > 2. [file:32]
    return alpha * r * r + beta * std::pow(r, p);
}

static double compute_vt(const WorkloadPlanes& planes) {
    // Weight planes; PFAS/cold planes could be added later, here energy-centric mix. [file:32]
    const double w_energy     = 0.35;
    const double w_carbon     = 0.25;
    const double w_hydraulics = 0.15;
    const double w_materials  = 0.15;
    const double w_dataq      = 0.10;
    const double alpha = 0.5;
    const double beta  = 0.5;
    const double p     = 4.0;

    double v =
        w_energy     * lyapunov_g(planes.r_energy,     alpha, beta, p) +
        w_carbon     * lyapunov_g(planes.r_carbon,     alpha, beta, p) +
        w_hydraulics * lyapunov_g(planes.r_hydraulics, alpha, beta, p) +
        w_materials  * lyapunov_g(planes.r_materials,  alpha, beta, p) +
        w_dataq      * lyapunov_g(planes.r_dataquality,alpha, beta, p);

    return v;
}

static KerTriad derive_ker(const WorkloadPlanes& planes, double vt_curr) {
    // Simple diagnostic KER: high K/E when planes are low and vt is contractive. [file:2][file:13]
    double avg_r =
        (planes.r_energy + planes.r_carbon + planes.r_hydraulics +
         planes.r_materials + planes.r_dataquality) / 5.0;

    KerTriad ker{};
    ker.k = std::max(0.0, 1.0 - avg_r * 0.2);
    ker.e = std::max(0.0, 1.0 - vt_curr * 0.1);
    ker.r = std::min(1.0, avg_r + vt_curr * 0.1);
    return ker;
}

WorkloadResult compute_workload_window(
    double power_w,
    double duration_s,
    double baseline_power_w,
    double baseline_duration_s,
    const WorkloadPlanes& planes_prev
) {
    validate_plane(planes_prev.r_energy,     "r_energy");
    validate_plane(planes_prev.r_carbon,     "r_carbon");
    validate_plane(planes_prev.r_hydraulics, "r_hydraulics");
    validate_plane(planes_prev.r_materials,  "r_materials");
    validate_plane(planes_prev.r_dataquality,"r_dataquality");

    WorkloadResult res{};
    res.energyreq_j       = power_w * duration_s;
    res.energy_j_baseline = baseline_power_w * baseline_duration_s;
    res.delta_energy_j    = res.energyreq_j - res.energy_j_baseline;

    res.vt_prev = compute_vt(planes_prev);

    WorkloadPlanes planes_curr = planes_prev;
    // Simple normalization: energy risk grows with relative energy increase.
    double rel = res.delta_energy_j / std::max(res.energy_j_baseline, 1.0);
    planes_curr.r_energy = std::min(1.0, std::max(0.0, planes_prev.r_energy + rel * 0.1));

    res.vt_curr    = compute_vt(planes_curr);
    res.delta_vt   = res.vt_curr - res.vt_prev;
    res.planes     = planes_curr;
    res.ker        = derive_ker(planes_curr, res.vt_curr);
    return res;
}

int main() {
    try {
        WorkloadPlanes planes_prev{0.18, 0.16, 0.17, 0.14, 0.10};
        WorkloadResult res = compute_workload_window(
            100.0,      // power W
            3600.0,     // 1 hour
            91.7,       // baseline W
            3600.0,
            planes_prev
        );

        std::cout << "energyreq_j=" << res.energyreq_j << "\n";
        std::cout << "energy_j_baseline=" << res.energy_j_baseline << "\n";
        std::cout << "delta_energy_j=" << res.delta_energy_j << "\n";
        std::cout << "vt_prev=" << res.vt_prev << "\n";
        std::cout << "vt_curr=" << res.vt_curr << "\n";
        std::cout << "delta_vt=" << res.delta_vt << "\n";
        std::cout << "k=" << res.ker.k << " e=" << res.ker.e << " r=" << res.ker.r << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
