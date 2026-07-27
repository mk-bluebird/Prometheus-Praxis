// filename: ecorestorationshard/cyboquaticprogress/20260725/cpp/blast_surcharge_kernel.cpp
// destination: ecorestorationshard/cyboquaticprogress/20260725/cpp/blast_surcharge_kernel.cpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

#include <cmath>
#include <cstdint>

struct BlastSurchargeInput {
    double surcharge_index;     // 0..1 dimensionless surcharge severity [file:2]
    double flow_m3s;            // flow at breach segment
    double head_loss_m;         // head loss across local reach
    double bulk_density_kg_m3;  // effective water+sediment density
    double canal_width_m;       // local wetted width
    double corridor_radius_m;   // corridor-safe radius baseline [file:2]
};

struct BlastSurchargeOutput {
    double blast_radius_m;   // predicted radial extent
    double vt_residual;      // normalized Lyapunov slice 0..1
    double r_energy;         // risk coordinates 0..1
    double r_hydraulics;
    double r_bio;
    double r_tox;
    double r_uncertainty;
    double r_topology;
};

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Simple non-actuating diagnostic kernel:
// - Approximates blast radius from surcharge_index, flow, head loss, width.
// - Constructs risk coordinates and a residual Vt = Σ w_j r_j^2.
// - Designed to be called from Java/SQL telemetry, not from actuators. [file:2][file:7]
extern "C" int compute_blast_surcharge_kernel(
    const BlastSurchargeInput* in,
    BlastSurchargeOutput* out
) {
    if (!in || !out) {
        return -1;
    }

    // Approximate kinetic energy proxy (no units enforcement; purely diagnostic).
    // E ~ 0.5 * ρ * Q^2 / W, scaled by head loss as severity.
    const double rho = in->bulk_density_kg_m3;
    const double Q   = in->flow_m3s;
    const double W   = (in->canal_width_m > 0.0) ? in->canal_width_m : 1.0;
    const double H   = (in->head_loss_m > 0.0) ? in->head_loss_m : 0.0;

    double energy_proxy = 0.5 * rho * Q * Q / W;
    energy_proxy *= (1.0 + H); // amplify by head loss severity

    // Normalize energy_proxy into a 0..1 risk coordinate using a heuristic scale.
    const double energy_scale = rho * 10.0; // heuristic, consistent with corridor tuning tasks [file:2]
    double r_energy = clamp01(energy_proxy / energy_scale);

    // Hydraulics risk from surcharge_index and head loss.
    double r_hydraulics = clamp01(0.7 * in->surcharge_index + 0.3 * clamp01(H / 2.0));

    // Bio and tox risks are placeholders: they should be tightened using BOD/TSS/CEC and PFAS data. [file:2]
    double r_bio = clamp01(0.5 * in->surcharge_index);
    double r_tox = clamp01(0.4 * in->surcharge_index);

    // Uncertainty plane reflects poor telemetry; here we assume moderate trust.
    double r_uncertainty = 0.2;

    // Topology risk placeholder: elevated for high surcharge and head loss (graph fragility). [file:2]
    double r_topology = clamp01(0.5 * in->surcharge_index + 0.2 * clamp01(H / 2.0));

    // Blast radius proportional to surcharge_index, flow-derived velocity, and head loss.
    double velocity_m_s = (Q > 0.0 && W > 0.0) ? (Q / W) : 0.0;
    double radius = in->corridor_radius_m
                    * (1.0 + 0.8 * in->surcharge_index)
                    * (1.0 + 0.3 * clamp01(velocity_m_s / 2.0))
                    * (1.0 + 0.2 * clamp01(H / 2.0));

    if (radius < 0.0) {
        radius = 0.0;
    }

    // Lyapunov residual slice Vt = Σ w_j r_j^2 with weights from drainage/workload grammar. [file:2]
    const double w_energy      = 0.30;
    const double w_hydraulics  = 0.30;
    const double w_bio         = 0.15;
    const double w_tox         = 0.15;
    const double w_uncertainty = 0.05;
    const double w_topology    = 0.05;

    double vt =
        w_energy      * r_energy      * r_energy +
        w_hydraulics  * r_hydraulics  * r_hydraulics +
        w_bio         * r_bio         * r_bio +
        w_tox         * r_tox         * r_tox +
        w_uncertainty * r_uncertainty * r_uncertainty +
        w_topology    * r_topology    * r_topology;

    // Clamp vt into 0..1 corridor; global Vt chain is handled in Rust/SQL. [file:2]
    vt = clamp01(vt);

    out->blast_radius_m = radius;
    out->vt_residual    = vt;
    out->r_energy       = r_energy;
    out->r_hydraulics   = r_hydraulics;
    out->r_bio          = r_bio;
    out->r_tox          = r_tox;
    out->r_uncertainty  = r_uncertainty;
    out->r_topology     = r_topology;

    return 0;
}
