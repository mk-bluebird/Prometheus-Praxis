// file: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/cpp/canal_blastradius_engine.cpp
// destination: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/cpp/canal_blastradius_engine.cpp
// purpose: Non-actuating C++ numeric kernel to compute canal surcharge blast radii and KER-oriented
//          risk coordinates, for ingestion into SQLite via Java telemetry. Energy-efficient, diagnostic only [file:4][file:18].

#include <cmath>
#include <string>
#include <iostream>

struct CanalNodeEnvelope {
    std::string canal_node_id;
    double      max_diag_energy_j;
    double      topo_sensitivity;   // 0-1
};

struct SurchargeEventInput {
    std::string surcharge_event_id;
    std::string canal_node_id;
    double      surcharge_m;        // m
    double      hydraulic_head_m;   // m
    double      diag_energy_j;      // J
    double      data_quality;       // 0-1
};

struct BlastRadiusOutput {
    std::string blast_diag_id;
    double      radius_m;
    double      r_hydraulics;
    double      r_energy;
    double      r_topology;
    double      r_biodiversity;
    double      vt_residual;
    double      k_knowledge_factor;
    double      e_eco_impact;
    double      r_risk_factor;
    double      ker_score;
    double      energy_per_m_j;
};

/**
 * Normalize a scalar into [0,1] with linear mapping and clamping.
 */
static double normalize(double value, double min_val, double max_val) {
    if (max_val <= min_val) {
        return 0.0;
    }
    double x = (value - min_val) / (max_val - min_val);
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return x;
}

/**
 * Compute a simple hydraulic blast radius based on surcharge and hydraulic head.
 * Radius grows with sqrt(energy density) but is bounded; non-actuating diagnostic only.
 */
static double compute_radius_m(const SurchargeEventInput &ev,
                               const CanalNodeEnvelope &env) {
    // Effective energy scalar using surcharge and head; scaled to meters.
    double effective_energy = ev.surcharge_m * ev.hydraulic_head_m;
    if (effective_energy <= 0.0) {
        return 0.0;
    }
    // Topo sensitivity shrinks radius for sensitive terrain to favor conservative corridors.
    double base_radius = std::sqrt(effective_energy);
    double radius      = base_radius * (1.0 - env.topo_sensitivity * 0.5);
    if (radius < 0.0) radius = 0.0;
    return radius;
}

/**
 * Compute risk coordinates and residual from event + envelope.
 * Planes: hydraulics, energy, topology, biodiversity (topology proxy used for biodiversity).
 */
static BlastRadiusOutput compute_blast_radius_diag(const std::string &blast_diag_id,
                                                   const SurchargeEventInput &ev,
                                                   const CanalNodeEnvelope &env) {
    BlastRadiusOutput out{};
    out.blast_diag_id = blast_diag_id;

    // Radius in meters.
    out.radius_m = compute_radius_m(ev, env);

    // Hydraulics risk: normalize surcharge depth against node max_surcharge_m surrogate.
    // Here we assume a reference safe_max_surcharge = env.topo_sensitivity-weighted bound.
    double safe_max_surcharge = std::max(0.5, 2.0 - env.topo_sensitivity); // m
    out.r_hydraulics = normalize(ev.surcharge_m, 0.0, safe_max_surcharge);

    // Energy risk: fraction of diag_energy_j vs max_diag_energy_j.
    double max_energy = (env.max_diag_energy_j > 0.0) ? env.max_diag_energy_j : ev.diag_energy_j;
    out.r_energy = normalize(ev.diag_energy_j, 0.0, max_energy);

    // Topology risk: direct use of topo_sensitivity (0-1).
    out.r_topology = env.topo_sensitivity;

    // Biodiversity risk: proxy from hydraulics and topology (average).
    out.r_biodiversity = 0.5 * (out.r_hydraulics + out.r_topology);

    // Lyapunov residual Vt = Σ w_j r_j^2 with band-agnostic weights [file:18][file:31].
    const double w_h = 0.35;
    const double w_e = 0.25;
    const double w_t = 0.20;
    const double w_b = 0.20;

    out.vt_residual =
        w_h * out.r_hydraulics * out.r_hydraulics +
        w_e * out.r_energy      * out.r_energy      +
        w_t * out.r_topology    * out.r_topology    +
        w_b * out.r_biodiversity* out.r_biodiversity;

    // KER scoring: high K when data_quality is high and residual is low,
    // high E when radius_m is small per energy used, R tracks residual [file:18][file:31].
    out.k_knowledge_factor = std::max(0.0, std::min(1.0, ev.data_quality * (1.0 - out.vt_residual)));
    double energy_per_m = (out.radius_m > 0.0) ? (ev.diag_energy_j / out.radius_m) : ev.diag_energy_j;
    out.energy_per_m_j  = energy_per_m;

    // Eco-impact: favor small radius and low residual.
    double eco_raw = (1.0 - out.vt_residual) * (out.radius_m > 0.0 ? 1.0 / (1.0 + out.radius_m) : 1.0);
    if (eco_raw < 0.0) eco_raw = 0.0;
    if (eco_raw > 1.0) eco_raw = 1.0;
    out.e_eco_impact = eco_raw;

    // Risk-of-harm coordinate R: clamp residual into [0,1].
    out.r_risk_factor = std::max(0.0, std::min(1.0, out.vt_residual));

    // Composite KER score k*e - r.
    out.ker_score = out.k_knowledge_factor * out.e_eco_impact - out.r_risk_factor;

    return out;
}

/**
 * Simple CLI: read one surcharge event from stdin and emit a blast-radius diagnostic as CSV.
 * This stays non-actuating and can be piped into Java/SQL ingestion.
 *
 * Input format (whitespace separated):
 *   surcharge_event_id canal_node_id surcharge_m hydraulic_head_m diag_energy_j data_quality topo_sensitivity max_diag_energy_j
 *
 * Output: CSV line
 *   blast_diag_id,radius_m,r_hydraulics,r_energy,r_topology,r_biodiversity,vt_residual,k,e,r,ker_score,energy_per_m_j
 */
int main() {
    std::string surcharge_event_id;
    std::string canal_node_id;
    double surcharge_m;
    double hydraulic_head_m;
    double diag_energy_j;
    double data_quality;
    double topo_sensitivity;
    double max_diag_energy_j;

    if (!(std::cin >> surcharge_event_id >> canal_node_id
          >> surcharge_m >> hydraulic_head_m
          >> diag_energy_j >> data_quality
          >> topo_sensitivity >> max_diag_energy_j)) {
        std::cerr << "Invalid input for surcharge event\n";
        return 1;
    }

    SurchargeEventInput ev{
        surcharge_event_id,
        canal_node_id,
        surcharge_m,
        hydraulic_head_m,
        diag_energy_j,
        data_quality
    };
    CanalNodeEnvelope env{
        canal_node_id,
        max_diag_energy_j,
        topo_sensitivity
    };

    std::string blast_diag_id = surcharge_event_id + "-BR";
    BlastRadiusOutput out = compute_blast_radius_diag(blast_diag_id, ev, env);

    // CSV output: ready for ingestion; no actuation here.
    std::cout << out.blast_diag_id << ","
              << out.radius_m << ","
              << out.r_hydraulics << ","
              << out.r_energy << ","
              << out.r_topology << ","
              << out.r_biodiversity << ","
              << out.vt_residual << ","
              << out.k_knowledge_factor << ","
              << out.e_eco_impact << ","
              << out.r_risk_factor << ","
              << out.ker_score << ","
              << out.energy_per_m_j
              << "\n";

    return 0;
}
