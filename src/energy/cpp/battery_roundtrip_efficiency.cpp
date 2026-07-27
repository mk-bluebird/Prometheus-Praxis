// filename: src/energy/cpp/battery_roundtrip_efficiency.cpp
// destination: Prometheus-Praxis/src/energy/cpp/battery_roundtrip_efficiency.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Degradation-adjusted round-trip efficiency model inputs.[file:80]
struct RoundTripEfficiencyInput {
    double eta_initial;      // initial round-trip efficiency (0..1).
    double eta_floor;        // minimum efficiency at end-of-life (0..1).
    double cycles;           // total equivalent full cycles modeled.
    double degradation_beta; // curvature parameter for efficiency decay.[file:80]
    double Ex_target;        // target Ex ratio (delivered/charged) for scheduling.[file:80]
};

// Corridor bands for energy degradation risk renergydeg.[file:80]
struct EnergyDegCorridor {
    double safe;  // safe band upper bound for degradation index.
    double gold;  // gold band upper bound.
    double hard;  // hard band upper bound.
};

// Output per cycle step.[file:80]
struct RoundTripEfficiencyOutput {
    double eta_rt;      // current round-trip efficiency (0..1).
    double Ex_ratio;    // delivered/charged energy ratio (0..1).
    double renergydeg;  // normalized degradation risk coordinate (0..1).
};

// Safegoldhard normalization for renergydeg.[file:80]
static double normalize_renergydeg(double x, const EnergyDegCorridor& band)
{
    if (!std::isfinite(x)) {
        return 1.0;
    }
    if (x <= band.safe) {
        return 0.0;
    }
    if (x >= band.hard) {
        return 1.0;
    }
    if (x <= band.gold) {
        const double t = (x - band.safe) / (band.gold - band.safe);
        return 0.5 * t;
    }
    const double t = (x - band.gold) / (band.hard - band.gold);
    return 0.5 + 0.5 * t;
}

// Simple monotone efficiency decay curve over normalized cycle fraction.[file:80]
static double efficiency_at_cycle(const RoundTripEfficiencyInput& in,
                                  double cycle_index)
{
    if (cycle_index <= 0.0) {
        return in.eta_initial;
    }
    const double n = in.cycles > 0.0 ? in.cycles : 1.0;
    const double f = cycle_index / n; // 0..1.[file:80]
    const double beta = in.degradation_beta;

    // Exponential-like decay from eta_initial toward eta_floor.[file:80]
    const double eta = in.eta_floor +
                       (in.eta_initial - in.eta_floor) * std::exp(-beta * f);

    if (eta < in.eta_floor) {
        return in.eta_floor;
    }
    if (eta > in.eta_initial) {
        return in.eta_initial;
    }
    return eta;
}

// Compute degradation index and Ex ratio at a given cycle.[file:80]
static RoundTripEfficiencyOutput compute_step(const RoundTripEfficiencyInput& in,
                                              const EnergyDegCorridor& band,
                                              double cycle_index)
{
    RoundTripEfficiencyOutput out{};

    const double eta = efficiency_at_cycle(in, cycle_index);
    out.eta_rt = eta;

    // Ex ratio as delivered/charged energy, approximated by eta.[file:80]
    out.Ex_ratio = eta;

    // Degradation index relative to initial eta and Ex_target.[file:80]
    const double loss = in.eta_initial - eta;
    const double target_loss = in.eta_initial - in.Ex_target;
    const double idx = target_loss > 0.0 ? loss / target_loss : loss;

    out.renergydeg = normalize_renergydeg(idx, band);
    return out;
}

// Main kernel: trajectory over cycles for energy-bank scheduling and Vt-gated control.[file:80]
extern "C" void battery_roundtrip_efficiency_run(const RoundTripEfficiencyInput* in,
                                                 const EnergyDegCorridor* band,
                                                 double cycle_index,
                                                 RoundTripEfficiencyOutput* out)
{
    if (!in || !band || !out) {
        return;
    }

    *out = compute_step(*in, *band, cycle_index);
}
