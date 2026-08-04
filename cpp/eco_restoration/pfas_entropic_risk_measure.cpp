// File: cpp/eco_restoration/pfas_entropic_risk_measure.cpp

#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <stdexcept>
#include <numeric>

namespace prometheus_praxis {
namespace eco_risk {

// Simple struct for Lyapunov residuals associated with PFAS cold-survival telemetry.
// residual_t is typically (observed Lyapunov increment - safe baseline).
struct LyapunovResidualSample {
    double residual_t;        // Lyapunov residual at time t (can be negative or positive)
    double temperature_c;     // ambient or water temperature in Celsius
    bool cold_survival_flag;  // true if PFAS-associated organism/material survived critical cold event
    double timestamp;         // normalized time (e.g., hours since reference)
};

// Entropic risk parameters: alpha controls risk aversion, epsilon controls residual normalization.
struct EntropicRiskParams {
    double alpha;   // risk aversion parameter (>0 for standard entropic risk)
    double epsilon; // small positive constant to stabilize residual normalization
};

// Entropic cumulative residual risk measure for a sequence of Lyapunov residuals.
// For a sequence {X_i}, define:
//   R_alpha(X) = (1 / alpha) * log( E[ exp(alpha * X_i_normalized) ] )
// where X_i_normalized = X_i / (1 + epsilon + |X_i|).
// This normalization saturates large residuals and makes the risk more robust to outliers.
class EntropicRiskMeasure {
public:
    explicit EntropicRiskMeasure(const EntropicRiskParams& params)
        : params_(params) {
        if (params_.alpha <= 0.0) {
            throw std::invalid_argument("alpha must be > 0");
        }
        if (params_.epsilon <= 0.0) {
            throw std::invalid_argument("epsilon must be > 0");
        }
    }

    // Compute normalized residual for a single sample.
    double normalizeResidual(double residual) const {
        double denom = 1.0 + params_.epsilon + std::fabs(residual);
        return residual / denom;
    }

    // Entropic risk for a vector of residuals.
    double entropicRisk(const std::vector<double>& residuals) const {
        if (residuals.empty()) {
            throw std::invalid_argument("residuals vector must not be empty");
        }
        double sum_exp = 0.0;
        for (double r : residuals) {
            double rn = normalizeResidual(r);
            sum_exp += std::exp(params_.alpha * rn);
        }
        double expectation = sum_exp / static_cast<double>(residuals.size());
        // For numerical stability, clamp expectation to a minimum positive value.
        if (expectation <= 0.0) {
            expectation = std::numeric_limits<double>::min();
        }
        return (1.0 / params_.alpha) * std::log(expectation);
    }

    // Entropic cumulative risk over time: running cumulative risk sequence.
    // For samples s_1,...,s_n, compute risk_k for k=1..n on prefix {s_1,...,s_k}.
    std::vector<double> cumulativeEntropicRisk(const std::vector<LyapunovResidualSample>& samples) const {
        if (samples.empty()) {
            throw std::invalid_argument("samples vector must not be empty");
        }
        std::vector<double> residuals;
        residuals.reserve(samples.size());
        std::vector<double> cumulative_risk(samples.size());

        for (std::size_t i = 0; i < samples.size(); ++i) {
            residuals.push_back(samples[i].residual_t);
            cumulative_risk[i] = entropicRisk(residuals);
        }
        return cumulative_risk;
    }

    // Sensitivity analysis: compute entropic risk for multiple alpha values while keeping epsilon fixed.
    // Returns vector of (alpha_value, risk_value) pairs.
    std::vector<std::pair<double, double>> sensitivityAlpha(
        const std::vector<double>& residuals,
        const std::vector<double>& alpha_grid) const {
        std::vector<std::pair<double, double>> result;
        result.reserve(alpha_grid.size());
        for (double a : alpha_grid) {
            if (a <= 0.0) continue;
            EntropicRiskParams p{a, params_.epsilon};
            EntropicRiskMeasure m(p);
            double risk = m.entropicRisk(residuals);
            result.emplace_back(a, risk);
        }
        return result;
    }

    // Sensitivity analysis: compute entropic risk for multiple epsilon values while keeping alpha fixed.
    std::vector<std::pair<double, double>> sensitivityEpsilon(
        const std::vector<double>& residuals,
        const std::vector<double>& epsilon_grid) const {
        std::vector<std::pair<double, double>> result;
        result.reserve(epsilon_grid.size());
        for (double e : epsilon_grid) {
            if (e <= 0.0) continue;
            EntropicRiskParams p{params_.alpha, e};
            EntropicRiskMeasure m(p);
            double risk = m.entropicRisk(residuals);
            result.emplace_back(e, risk);
        }
        return result;
    }

    // Approximate link between entropic risk and Lyapunov exponent sign:
    // If the residuals correspond to deviations from a baseline Lyapunov exponent λ0,
    // the average residual hadamard with sign(λ0) can be used to check coherence with risk.
    // This function returns an indicator of whether entropic risk and empirical Lyapunov sign
    // appear consistent (positive risk with positive average residual, etc.).
    bool isRiskConsistentWithLyapunovSign(const std::vector<double>& residuals,
                                          double baseline_lyapunov_exponent) const {
        if (residuals.empty()) return true;
        double risk = entropicRisk(residuals);
        double avg_residual = std::accumulate(residuals.begin(), residuals.end(), 0.0)
                              / static_cast<double>(residuals.size());
        int sign_lambda0 = (baseline_lyapunov_exponent > 0.0) ? 1 : (baseline_lyapunov_exponent < 0.0 ? -1 : 0);
        int sign_risk = (risk > 0.0) ? 1 : (risk < 0.0 ? -1 : 0);
        int sign_resid = (avg_residual > 0.0) ? 1 : (avg_residual < 0.0 ? -1 : 0);

        // Coherence: risk sign should track average residual sign and Lyapunov sign.
        // If λ0 < 0 (stable), we expect lower entropic risk; if λ0 > 0 (unstable), higher risk.
        if (sign_lambda0 == 0) {
            // baseline neutral: require risk sign to match residual sign
            return sign_risk == sign_resid;
        }
        return (sign_risk == sign_resid) && (sign_resid == sign_lambda0);
    }

private:
    EntropicRiskParams params_;
};

// Utility to compute binary cold-survival vs continuous risk correlation.
// For simplicity, we compute Spearman-like monotone association by counting concordant pairs.
double monotoneAssociationColdSurvival(
    const std::vector<LyapunovResidualSample>& samples,
    const EntropicRiskMeasure& measure) {
    if (samples.size() < 2) return 0.0;
    std::vector<double> residuals;
    residuals.reserve(samples.size());
    for (const auto& s : samples) {
        residuals.push_back(s.residual_t);
    }
    double risk = measure.entropicRisk(residuals);

    // Compare each sample's residual to overall risk: higher residual than risk threshold
    // should align with more frequent cold survival failures (flag=false).
    std::size_t concordant = 0;
    std::size_t discordant = 0;
    for (const auto& s : samples) {
        bool high_residual = (s.residual_t > risk);
        bool failed_survival = !s.cold_survival_flag;
        if ((high_residual && failed_survival) || (!high_residual && !failed_survival)) {
            concordant++;
        } else {
            discordant++;
        }
    }
    std::size_t total = concordant + discordant;
    if (total == 0) return 0.0;
    return static_cast<double>(concordant - discordant) / static_cast<double>(total);
}

// Example SQL artifacts for PFAS entropic risk computation. These are emitted as strings
// that can be installed into the database by deployment tooling or manual migration.
std::string sqlViewNormalizedLyapunovResiduals() {
    return R"SQL(
CREATE VIEW IF NOT EXISTS ker_pfashorizon_lyapunov_normalized AS
SELECT
    t.device_id,
    t.event_time,
    t.lyapunov_residual,
    t.cold_survival_flag,
    t.temperature_c,
    CASE
        WHEN ABS(t.lyapunov_residual) IS NULL THEN 0.0
        ELSE t.lyapunov_residual / (1.0 + :epsilon + ABS(t.lyapunov_residual))
    END AS lyapunov_residual_norm
FROM ker_pfashorizon_telemetry t;
)SQL";
}

// Cumulative sum window query over normalized residuals; materialize into ker_pfashorizon_r.
std::string sqlMaterializeEntropicRiskTable() {
    return R"SQL(
CREATE TABLE IF NOT EXISTS ker_pfashorizon_r AS
SELECT
    v.device_id,
    v.event_time,
    v.lyapunov_residual_norm,
    SUM(v.lyapunov_residual_norm) OVER (
        PARTITION BY v.device_id
        ORDER BY v.event_time
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS cumulative_residual_norm
FROM ker_pfashorizon_lyapunov_normalized v;
)SQL";
}

// Stored procedure / scheduled job: here expressed as a SQL function plus a recompute command.
std::string sqlRecomputeRiskProcedure() {
    return R"SQL(
-- Recompute PFAS entropic risk horizon ker_pfashorizon_r.
-- This function truncates and rebuilds the materialized table using latest telemetry.
CREATE OR REPLACE FUNCTION ker_pfashorizon_r_recompute(alpha DOUBLE PRECISION, epsilon DOUBLE PRECISION)
RETURNS VOID AS $$
BEGIN
    TRUNCATE TABLE ker_pfashorizon_r;
    INSERT INTO ker_pfashorizon_r(device_id, event_time, lyapunov_residual_norm, cumulative_residual_norm)
    SELECT
        v.device_id,
        v.event_time,
        v.lyapunov_residual_norm,
        SUM(v.lyapunov_residual_norm) OVER (
            PARTITION BY v.device_id
            ORDER BY v.event_time
            ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
        ) AS cumulative_residual_norm
    FROM ker_pfashorizon_lyapunov_normalized v;
END;
$$ LANGUAGE plpgsql;
)SQL";
}

// Kotlin/Java-style service description for feeding ker_r component.
// In C++ we provide a simple scalar feeder interface that can be bound to JNI or gRPC.
class KerRiskScalarFeeder {
public:
    KerRiskScalarFeeder(const EntropicRiskMeasure& measure)
        : measure_(measure) {}

    // Compute scalar risk from Lyapunov residual samples and return value for ker_r.
    double computeScalarForKerR(const std::vector<LyapunovResidualSample>& samples) const {
        if (samples.empty()) {
            return 0.0;
        }
        std::vector<double> residuals;
        residuals.reserve(samples.size());
        for (const auto& s : samples) {
            residuals.push_back(s.residual_t);
        }
        return measure_.entropicRisk(residuals);
    }

    // Design alert signal based on risk threshold crossing.
    bool shouldTriggerAlert(const std::vector<LyapunovResidualSample>& samples,
                            double risk_threshold) const {
        double risk_scalar = computeScalarForKerR(samples);
        return risk_scalar > risk_threshold;
    }

private:
    const EntropicRiskMeasure& measure_;
};

} // namespace eco_risk
} // namespace prometheus_praxis

int main() {
    using namespace prometheus_praxis::eco_risk;

    // Example usage with synthetic data to demonstrate the PFAS entropic risk measure.
    EntropicRiskParams params{1.2, 0.01};
    EntropicRiskMeasure measure(params);

    std::vector<LyapunovResidualSample> samples = {
        {0.05, -2.0, true, 0.0},
        {0.10, -4.0, true, 1.0},
        {0.20, -6.0, false, 2.0},
        {0.15, -5.5, false, 3.0},
        {-0.02, -1.0, true, 4.0}
    };

    std::vector<double> residuals;
    for (const auto& s : samples) {
        residuals.push_back(s.residual_t);
    }

    double risk = measure.entropicRisk(residuals);
    std::cout << "Entropic PFAS cold-survival risk scalar: " << risk << std::endl;

    auto cum_risk = measure.cumulativeEntropicRisk(samples);
    std::cout << "Cumulative entropic risk sequence:" << std::endl;
    for (std::size_t i = 0; i < cum_risk.size(); ++i) {
        std::cout << "  t=" << samples[i].timestamp << " risk=" << cum_risk[i] << std::endl;
    }

    double association = monotoneAssociationColdSurvival(samples, measure);
    std::cout << "Monotone association (risk vs cold survival): " << association << std::endl;

    double lambda0 = 0.05; // baseline Lyapunov exponent (example)
    bool coherent = measure.isRiskConsistentWithLyapunovSign(residuals, lambda0);
    std::cout << "Risk consistent with Lyapunov sign: " << (coherent ? "yes" : "no") << std::endl;

    KerRiskScalarFeeder feeder(measure);
    double ker_r_scalar = feeder.computeScalarForKerR(samples);
    std::cout << "ker_r scalar for PFAS horizon: " << ker_r_scalar << std::endl;
    bool alert = feeder.shouldTriggerAlert(samples, 0.10);
    std::cout << "Alert threshold crossing: " << (alert ? "trigger" : "no trigger") << std::endl;

    // Emit SQL artifacts for deployment inspection.
    std::cout << "\n--- SQL view for normalized Lyapunov residuals ---\n";
    std::cout << sqlViewNormalizedLyapunovResiduals() << std::endl;

    std::cout << "\n--- SQL materialization into ker_pfashorizon_r ---\n";
    std::cout << sqlMaterializeEntropicRiskTable() << std::endl;

    std::cout << "\n--- SQL stored procedure for risk recompute ---\n";
    std::cout << sqlRecomputeRiskProcedure() << std::endl;

    return 0;
}
