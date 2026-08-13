// File: cpp/eco_restoration/praxis_governance_kernel.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace prometheus::eco_restoration {

enum class Decision { Allow, Warn, Stop };

struct Corridor {
    std::vector<std::int64_t> lower;
    std::vector<std::int64_t> upper;

    bool contains(const std::vector<std::int64_t>& x) const {
        if (x.size() != lower.size() || lower.size() != upper.size()) return false;
        for (std::size_t i = 0; i < x.size(); ++i)
            if (x[i] < lower[i] || x[i] > upper[i]) return false;
        return true;
    }
};

using Matrix = std::vector<std::vector<std::int64_t>>;

inline std::int64_t quadratic(const Matrix& p, const std::vector<std::int64_t>& x) {
    std::int64_t total = 0;
    for (std::size_t i = 0; i < x.size(); ++i)
        for (std::size_t j = 0; j < x.size(); ++j)
            total += x[i] * p[i][j] * x[j];
    return total;
}

inline std::vector<std::int64_t> multiply(const Matrix& m,
                                          const std::vector<std::int64_t>& x,
                                          std::int64_t scale) {
    std::vector<std::int64_t> out(m.size(), 0);
    for (std::size_t i = 0; i < m.size(); ++i) {
        for (std::size_t j = 0; j < x.size(); ++j) out[i] += m[i][j] * x[j];
        out[i] /= scale;
    }
    return out;
}

inline bool symmetric_positive_definite(const Matrix& p) {
    const std::size_t n = p.size();
    if (n == 0) return false;
    for (const auto& row : p) if (row.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            if (p[i][j] != p[j][i]) return false;

    std::vector<std::vector<long double>> l(n, std::vector<long double>(n, 0.0L));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            long double sum = static_cast<long double>(p[i][j]);
            for (std::size_t k = 0; k < j; ++k) sum -= l[i][k] * l[j][k];
            if (i == j) {
                if (sum <= 0.0L) return false;
                l[i][j] = std::sqrt(sum);
            } else {
                if (l[j][j] == 0.0L) return false;
                l[i][j] = sum / l[j][j];
            }
        }
    }
    return true;
}

struct FiniteFieldLyapunovModel {
    Matrix p;
    Matrix closed_loop;
    std::int64_t scale = 1;
    Corridor corridor;

    bool strictly_decreases_for(const std::vector<std::int64_t>& x) const {
        if (scale <= 0 || !corridor.contains(x) || !symmetric_positive_definite(p)) return false;
        const auto next = multiply(closed_loop, x, scale);
        return corridor.contains(next) && quadratic(p, next) < quadratic(p, x);
    }

    bool exhaustively_proves_strict_decrease() const {
        if (corridor.lower.size() != corridor.upper.size() || corridor.lower.empty()) return false;
        std::vector<std::int64_t> state = corridor.lower;
        while (true) {
            if (!strictly_decreases_for(state)) return false;
            std::size_t i = 0;
            while (i < state.size() && state[i] == corridor.upper[i]) {
                state[i] = corridor.lower[i];
                ++i;
            }
            if (i == state.size()) return true;
            ++state[i];
        }
    }
};

struct NormalizedImpact {
    double knowledge_adequacy;
    double ecological_harm;
    double operational_risk;
    double neurorights_risk;
    double demographic_vulnerability;
};

struct ImpactWeights {
    double knowledge_gap = 0.15;
    double ecological_harm = 0.25;
    double operational_risk = 0.20;
    double neurorights_risk = 0.25;
    double demographic_vulnerability = 0.15;
};

inline double vulnerable_impact_potential(const NormalizedImpact& x,
                                          const ImpactWeights& w = {}) {
    const auto unit = [](double v) { return std::clamp(v, 0.0, 1.0); };
    const double base =
        w.knowledge_gap * (1.0 - unit(x.knowledge_adequacy)) +
        w.ecological_harm * unit(x.ecological_harm) +
        w.operational_risk * unit(x.operational_risk) +
        w.neurorights_risk * unit(x.neurorights_risk) +
        w.demographic_vulnerability * unit(x.demographic_vulnerability);
    const double interaction =
        0.20 * unit(x.neurorights_risk) * unit(x.demographic_vulnerability) +
        0.10 * unit(x.ecological_harm) * unit(x.demographic_vulnerability);
    return std::clamp(base + interaction, 0.0, 1.0);
}

struct DecisionLog {
    Decision decision;
    double phi;
    std::string policy_revision;
    std::string private_input_commitment;
};

struct ProofRequest {
    std::string guest_program_id;
    std::string public_journal;
    std::string private_input_commitment;
    std::string target_chain;
};

class PraxisGovernanceKernel {
public:
    explicit PraxisGovernanceKernel(double phi_critical) : phi_critical_(phi_critical) {
        if (phi_critical < 0.0 || phi_critical > 1.0) throw std::invalid_argument("invalid threshold");
    }

    DecisionLog decide(bool ker_envelopes_ok, bool roh_ceiling_ok, bool corridor_ok,
                       const NormalizedImpact& impact, std::string commitment) const {
        const double phi = vulnerable_impact_potential(impact);
        const bool invariants_ok = ker_envelopes_ok && roh_ceiling_ok && corridor_ok;
        const Decision d = phi > phi_critical_ ? Decision::Stop :
                           invariants_ok ? Decision::Allow : Decision::Warn;
        return {d, phi, "praxis-governance-v1", std::move(commitment)};
    }

    ProofRequest spawn_proof_request(const DecisionLog& log) const {
        const char* label = log.decision == Decision::Allow ? "Allow" :
                            log.decision == Decision::Warn ? "Warn" : "Stop";
        return {"praxis-aln-proof-guest-v1",
                std::string("{\"decision\":\"") + label + "\",\"phi\":" +
                    std::to_string(log.phi) + ",\"policy\":\"" + log.policy_revision + "\"}",
                log.private_input_commitment, "VeritasChain"};
    }

private:
    double phi_critical_;
};

} // namespace prometheus::eco_restoration
