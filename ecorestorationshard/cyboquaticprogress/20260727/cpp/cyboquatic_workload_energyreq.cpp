// filename: ecorestorationshard/cyboquaticprogress/20260727/cpp/cyboquatic_workload_energyreq.cpp
// purpose: Non-actuating Cyboquatic workload energetics kernel computing energyreqJ and ΔVt
// domain: (d) Cyboquatic workload (energyreqJ, ΔVt)
// anchor: logicalname=PHXWORKLOADENERGYDV20260727, evidencehex=0x20260727PHX3345NWorkloadEnergyDeltaVt

#include <cmath>
#include <cstdio>
#include <cstdlib>

struct WorkloadInput {
    double energy_req_j;    // Required energy for workload segment [J]
    double energy_surplus_j; // Surplus (tailwind) energy available [J]
    double hydraulic_risk;  // Normalized hydraulic risk 0..1 (e.g., surcharge, velocity)
    double uncertainty_risk; // Normalized telemetry/model uncertainty 0..1
};

struct RiskVector {
    double r_energy;       // 0..1
    double r_hydraulics;   // 0..1
    double r_uncertainty;  // 0..1
};

struct ResidualSlice {
    RiskVector risk;
    double vt_before;
    double vt_after;
    double delta_vt;
};

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Map tailwind ratio Rt = Esurplus / Ereq into r_energy per Phoenix workload corridor.
// Strong tailwind (Rt >= 1.0) => r_energy ~ 0, severe shortfall (Rt <= 0.0) => r_energy ~ 1.
static double map_tailwind_to_r_energy(double energy_req_j, double energy_surplus_j) {
    if (energy_req_j <= 0.0) {
        // No meaningful workload, treat as neutral risk for energy.
        return 0.5;
    }
    double rt = energy_surplus_j / energy_req_j;
    if (rt >= 1.0) {
        return 0.0;
    }
    if (rt <= 0.0) {
        return 1.0;
    }
    // Linear interpolation between strong tailwind and severe shortfall
    double r = 1.0 - rt; // rt in (0,1) => r in (1,0)
    return clamp01(r);
}

// Quadratic Lyapunov residual Vt = sum_j w_j * r_j^2 with Phoenix workload weights.
// ENERGY and HYDRAULICS planes are non-offsettable; UNCERTAINTY is significant but secondary.
static double compute_vt(const RiskVector &rv) {
    const double w_energy      = 0.50;
    const double w_hydraulics  = 0.30;
    const double w_uncertainty = 0.20;
    double v =
        w_energy      * rv.r_energy      * rv.r_energy +
        w_hydraulics  * rv.r_hydraulics  * rv.r_hydraulics +
        w_uncertainty * rv.r_uncertainty * rv.r_uncertainty;
    return v;
}

// Non-actuating workload kernel: normalize risks, compute Vt before/after, and ΔVt.
// vt_before is supplied from prior day or prior corridor; vt_after is computed from current risks.
ResidualSlice cyboquatic_compute_workload_slice(const WorkloadInput &in, double vt_before) {
    ResidualSlice slice;
    slice.risk.r_energy      = map_tailwind_to_r_energy(in.energy_req_j, in.energy_surplus_j);
    slice.risk.r_hydraulics  = clamp01(in.hydraulic_risk);
    slice.risk.r_uncertainty = clamp01(in.uncertainty_risk);

    slice.vt_before = vt_before;
    slice.vt_after  = compute_vt(slice.risk);
    slice.delta_vt  = slice.vt_after - slice.vt_before;
    return slice;
}

// Simple CLI diagnostic for daily cyboquatic progress; prints non-actuating telemetry only.
int main(int argc, char **argv) {
    if (argc < 5) {
        std::fprintf(stderr,
            "Usage: %s energy_req_J energy_surplus_J hydraulic_risk uncertainty_risk\n",
            argc > 0 ? argv[0] : "cyboquatic_workload_energyreq");
        return 1;
    }

    WorkloadInput in;
    in.energy_req_j     = std::atof(argv[1]);
    in.energy_surplus_j = std::atof(argv[2]);
    in.hydraulic_risk   = std::atof(argv[3]);
    in.uncertainty_risk = std::atof(argv[4]);

    const double vt_before = 0.0; // For a single-day slice; prior Vt injected by SQL in practice.
    ResidualSlice slice = cyboquatic_compute_workload_slice(in, vt_before);

    std::printf("PHXWORKLOADENERGYDV20260727 evidencehex=0x20260727PHX3345NWorkloadEnergyDeltaVt\n");
    std::printf("energy_req_J=%.6f energy_surplus_J=%.6f\n", in.energy_req_j, in.energy_surplus_j);
    std::printf("r_energy=%.6f r_hydraulics=%.6f r_uncertainty=%.6f\n",
                slice.risk.r_energy, slice.risk.r_hydraulics, slice.risk.r_uncertainty);
    std::printf("vt_before=%.6f vt_after=%.6f delta_vt=%.6f\n",
                slice.vt_before, slice.vt_after, slice.delta_vt);
    return 0;
}
