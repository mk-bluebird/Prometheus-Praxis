// filename: ecorestorationshard/ppx_aln/src/aln_cpp_bridge.hpp
// destination: ecorestorationshard/ppx_aln/src/aln_cpp_bridge.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose
// Thin, non-actuating C++ bridge for ALN v2 contracts and cyboquatic
// workloads. C++ code throughout ecorestorationshard can call these
// structs and functions without needing a full ALN parser; wiring to
// real hardware remains in separate, governed repos. [file:2][file:34]

#ifndef PPX_ALN_CPP_BRIDGE_HPP
#define PPX_ALN_CPP_BRIDGE_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace ppx_aln {

// Triad used across KER kernels.
struct KerTriad {
    double k;
    double e;
    double r;

    void validate() const {
        if (k < 0.0 || k > 1.0) throw std::runtime_error("K out of [0,1]");
        if (e < 0.0 || e > 1.0) throw std::runtime_error("E out of [0,1]");
        if (r < 0.0 || r > 1.0) throw std::runtime_error("R out of [0,1]");
    }
};

// Lyapunov residual plane weights, mirroring ALN PlaneWeightsShard. [file:32][file:34]
struct PlaneWeights {
    double w_energy;
    double w_hydraulics;
    double w_carbon;
    double w_biodiversity;
    double w_materials;
    double w_dataquality;

    void validate_nonnegative() const {
        if (w_energy     < 0.0 ||
            w_hydraulics < 0.0 ||
            w_carbon     < 0.0 ||
            w_biodiversity < 0.0 ||
            w_materials  < 0.0 ||
            w_dataquality < 0.0) {
            throw std::runtime_error("PlaneWeights must be non-negative");
        }
    }
};

// Risk planes for a workload snapshot, normalized in [0,1]. [file:2]
struct RiskPlanes {
    double r_energy;
    double r_hydraulics;
    double r_carbon;
    double r_biodiversity;
    double r_materials;
    double r_dataquality;

    void validate_unit_cube() const {
        const double vals[] = {
            r_energy, r_hydraulics, r_carbon,
            r_biodiversity, r_materials, r_dataquality
        };
        for (double v : vals) {
            if (v < 0.0 || v > 1.0) {
                throw std::runtime_error("Risk plane out of [0,1]");
            }
        }
    }
};

// SafeStep rule used to enforce V(t+1) <= V(t) + epsilon. [file:34]
struct SafeStepRule {
    double epsilon;     // numerical slack
    double vt_ceiling;  // optional residual ceiling in [0,1], 0 means "unused"

    void validate() const {
        if (epsilon < 0.0) throw std::runtime_error("epsilon must be >= 0");
        if (vt_ceiling < 0.0 || vt_ceiling > 1.0) {
            throw std::runtime_error("vt_ceiling must be in [0,1]");
        }
    }
};

// Deploy decision kernel thresholds, aligned with PROD gates. [file:34]
struct DeployDecisionKernel {
    double k_min; // e.g. 0.93
    double e_min; // e.g. 0.90
    double r_max; // e.g. 0.13

    void validate() const {
        if (k_min < 0.0 || k_min > 1.0) throw std::runtime_error("k_min out of [0,1]");
        if (e_min < 0.0 || e_min > 1.0) throw std::runtime_error("e_min out of [0,1]");
        if (r_max < 0.0 || r_max > 1.0) throw std::runtime_error("r_max out of [0,1]");
    }
};

// Compute Lyapunov residual V(t) = sum_j w_j * r_j^2 over planes. [file:32][file:34]
inline double compute_residual(const PlaneWeights& w, const RiskPlanes& r) {
    w.validate_nonnegative();
    r.validate_unit_cube();

    return
        w.w_energy      * r.r_energy      * r.r_energy +
        w.w_hydraulics  * r.r_hydraulics  * r.r_hydraulics +
        w.w_carbon      * r.r_carbon      * r.r_carbon +
        w.w_biodiversity* r.r_biodiversity* r.r_biodiversity +
        w.w_materials   * r.r_materials   * r.r_materials +
        w.w_dataquality * r.r_dataquality * r.r_dataquality;
}

// Safe-step gate: enforce V(t+1) <= V(t) + epsilon and optional ceiling. [file:34]
inline bool safestep_holds(const SafeStepRule& rule, double vt_prev, double vt_next) {
    rule.validate();
    if (vt_next > vt_prev + rule.epsilon) {
        return false;
    }
    if (rule.vt_ceiling > 0.0 && vt_next > rule.vt_ceiling) {
        return false;
    }
    return true;
}

// Simple KER mapping: high K/E when residual is low; R follows residual. [file:2][file:13]
inline KerTriad derive_ker(double vt, double vt_prev) {
    KerTriad ker{};
    double dv = vt - vt_prev;
    double r  = std::max(0.0, std::min(1.0, vt));

    double k = std::max(0.0, 1.0 - vt * 0.3);
    if (dv > 0.0) {
        k = std::max(0.0, k - 0.1);
    }

    double e = std::max(0.0, 1.0 - vt * 0.2);
    if (dv > 0.0) {
        e = std::max(0.0, e - 0.1);
    }

    ker.k = k;
    ker.e = e;
    ker.r = r;
    ker.validate();
    return ker;
}

// Deploy gate: usable for hardware-adjacent repos; decisions remain non-actuating
// inside ecorestorationshard, but PROD hardware repos can import this as a pure
// function. [file:34]
inline bool deploy_admissible(const DeployDecisionKernel& kernel,
                              const KerTriad& ker) {
    kernel.validate();
    ker.validate();
    if (ker.k < kernel.k_min) return false;
    if (ker.e < kernel.e_min) return false;
    if (ker.r > kernel.r_max) return false;
    return true;
}

// Non-research directory classifier: ppx_aln is RESEARCH/non-actuating inside
// Prometheus-Praxis; hardware-facing repos must live under EcoNet/Eco-Fort,
// import this header, and tag their manifests with lane != RESEARCH. [file:2][file:34]
inline bool is_non_research_directory(const std::string& repo_relative_path) {
    // Heuristic: any path under "ecorestorationshard/ppx_aln" is research-only.
    if (repo_relative_path.find("ecorestorationshard/ppx_aln") == 0) {
        return false;
    }
    // Directories like "EcoNet/hardware", "Eco-Fort/actuators" are non-research.
    if (repo_relative_path.find("EcoNet/hardware") == 0) return true;
    if (repo_relative_path.find("Eco-Fort/actuators") == 0) return true;
    return false;
}

} // namespace ppx_aln

#endif // PPX_ALN_CPP_BRIDGE_HPP
