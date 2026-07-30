// File: cpp/eco_restoration/psych_electrode_gp_and_hex_crosswalk_schema.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 45. Learning psych–electrode coupling with Gaussian process regression
// ----------------------------------------------------------
//
// We model an unknown nonlinear coupling f between psych state p and
// electrode reliability r (e.g., drift events, SNR) via GP regression:
//
//   y = f(x) + ε
//   x = (p, r),  y = observed drift or reliability metric.
//
// GP prior:
//   f(x) ~ GP(m(x), k(x, x'))
//
// with mean function m(x) (often 0) and kernel k(x, x').
//
// Given sparse drift-event data (x_i, y_i), we learn posterior mean and
// variance. The model's uncertainty σ_f^2(x) can then be used to adjust
// Kalman gain: higher uncertainty ⇒ lower gain to avoid over-trusting
// psych estimates in poorly learned regions.

struct DriftSample {
    double psych_state;    // p
    double reliability;    // r
    double drift_obs;      // y
};

struct GPParams {
    double length_scale_p;
    double length_scale_r;
    double signal_variance;
    double noise_variance;
};

// Squared exponential kernel k(x, x').
double gp_kernel(const DriftSample& a,
                 const DriftSample& b,
                 const GPParams& gp) {
    double dp = a.psych_state - b.psych_state;
    double dr = a.reliability  - b.reliability;
    double lp = gp.length_scale_p;
    double lr = gp.length_scale_r;
    double sq_norm = (dp*dp)/(lp*lp) + (dr*dr)/(lr*lr);
    return gp.signal_variance * std::exp(-0.5 * sq_norm);
}

// Compute posterior mean and variance at test point x_* given training data.
struct GPPrediction {
    double mean;
    double variance;
};

GPPrediction gp_predict(const DriftSample& x_star,
                        const std::vector<DriftSample>& train,
                        const GPParams& gp) {
    std::size_t n = train.size();
    if (n == 0) return GPPrediction{0.0, gp.signal_variance};

    // Build K matrix and k_* vector.
    std::vector<std::vector<double>> K(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            K[i][j] = gp_kernel(train[i], train[j], gp);
            if (i == j) {
                K[i][j] += gp.noise_variance;
            }
        }
    }

    std::vector<double> k_star(n);
    for (std::size_t i = 0; i < n; ++i) {
        k_star[i] = gp_kernel(x_star, train[i], gp);
    }

    // Solve linear system K α = y to get α (using naive Gaussian elimination).
    std::vector<std::vector<double>> M(n, std::vector<double>(n+1, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            M[i][j] = K[i][j];
        }
        M[i][n] = train[i].drift_obs;
    }

    for (std::size_t i = 0; i < n; ++i) {
        double pivot = M[i][i];
        if (std::fabs(pivot) < 1e-12) continue;
        for (std::size_t j = i; j <= n; ++j) {
            M[i][j] /= pivot;
        }
        for (std::size_t k = 0; k < n; ++k) {
            if (k == i) continue;
            double factor = M[k][i];
            for (std::size_t j = i; j <= n; ++j) {
                M[k][j] -= factor * M[i][j];
            }
        }
    }

    std::vector<double> alpha(n);
    for (std::size_t i = 0; i < n; ++i) {
        alpha[i] = M[i][n];
    }

    double mean = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mean += k_star[i] * alpha[i];
    }

    // Posterior variance:
    // var(f_*) = k(x_*, x_*) - k_*^T K^{-1} k_*
    // Approximate K^{-1} via solution to K v = k_*.
    std::vector<std::vector<double>> M2(n, std::vector<double>(n+1, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            M2[i][j] = K[i][j];
        }
        M2[i][n] = k_star[i];
    }

    for (std::size_t i = 0; i < n; ++i) {
        double pivot = M2[i][i];
        if (std::fabs(pivot) < 1e-12) continue;
        for (std::size_t j = i; j <= n; ++j) {
            M2[i][j] /= pivot;
        }
        for (std::size_t k = 0; k < n; ++k) {
            if (k == i) continue;
            double factor = M2[k][i];
            for (std::size_t j = i; j <= n; ++j) {
                M2[k][j] -= factor * M2[i][j];
            }
        }
    }

    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = M2[i][n];
    }

    double k_star_Kinv_k_star = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        k_star_Kinv_k_star += k_star[i] * v[i];
    }

    double k_star_star = gp_kernel(x_star, x_star, gp);
    double var = k_star_star - k_star_Kinv_k_star;
    if (var < 0.0) var = 0.0;

    return GPPrediction{mean, var};
}

// Dynamic Kalman gain adjustment:
//   - Baseline Kalman gain K_base computed from process/measurement noise.
//   - GP variance σ_f^2(x) used to scale gain:
//       K_dyn = K_base * g(σ_f^2)
//   - Example: g(σ) = 1 / (1 + σ / σ_ref), so higher uncertainty ⇒ smaller K_dyn.

double dynamic_kalman_gain(double K_base,
                           double gp_variance,
                           double sigma_ref) {
    double scale = 1.0 / (1.0 + gp_variance / sigma_ref);
    return K_base * scale;
}

// ----------------------------------------------------------
// 46. Legal-technical crosswalk automation: DSL schema
// ----------------------------------------------------------
//
// We define a DSL schema for hex-anchored standards that can be parsed
// by a Python script to generate cross-reference tables.
//
// DSL structure:
//
//   standard <HexAnchor> {
//       clause <ClauseId> {
//           text        "Human-readable clause text";
//           aln_field   "aln/shard/file::field_or_invariant";
//           rust_fn     "crate::module::function";
//           kani_harness "crate::tests::kani_harness_name";
//       }
//       ...
//   }
//
// Grammar (EBNF-style):
//
//   Standard      ::= "standard" HexAnchor "{" Clause* "}"
//   HexAnchor     ::= /0x[0-9A-Fa-f]+[A-Za-z0-9_]*/
//   Clause        ::= "clause" Ident "{" ClauseBody "}"
//   ClauseBody    ::= TextField ALNField RustField KaniField
//   TextField     ::= "text" StringLiteral ";"
//   ALNField      ::= "aln_field" StringLiteral ";"
//   RustField     ::= "rust_fn" StringLiteral ";"
//   KaniField     ::= "kani_harness" StringLiteral ";"
//   Ident         ::= /[A-Za-z0-9_]+/
//   StringLiteral ::= '"' .* '"'
//
// The Python script would:
//
//   1) Parse the DSL into an AST.
//   2) For each clause, extract clause_id, aln_field, rust_fn, kani_harness.
//   3) Emit a table (e.g., CSV/Markdown) with columns:
//        clause_id, hex_anchor, aln_field, rust_fn, kani_harness, text.
//
// Here we define a C++ data structure that mirrors this schema.

struct ClauseSpec {
    std::string clause_id;
    std::string text;
    std::string aln_field;
    std::string rust_fn;
    std::string kani_harness;
};

struct StandardSpec {
    std::string hex_anchor;
    std::vector<ClauseSpec> clauses;
};

void print_crosswalk(const StandardSpec& spec) {
    std::cout << "Hex-anchored standard crosswalk for " << spec.hex_anchor << ":\n\n";
    std::cout << std::left << std::setw(10) << "ClauseId"
              << std::setw(24) << "ALNField"
              << std::setw(30) << "RustFn"
              << std::setw(30) << "KaniHarness"
              << "Text\n";
    std::cout << std::string(110, '-') << "\n";
    for (const auto& c : spec.clauses) {
        std::cout << std::left << std::setw(10) << c.clause_id
                  << std::setw(24) << c.aln_field
                  << std::setw(30) << c.rust_fn
                  << std::setw(30) << c.kani_harness
                  << c.text << "\n";
    }
}

// Example instantiation for a labor-psych continuity standard.
StandardSpec make_example_standard() {
    StandardSpec spec;
    spec.hex_anchor = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    spec.clauses.push_back(ClauseSpec{
        "LP-ER-001",
        "Any labor-psych continuity claim SHALL present a reliability_token issued within the last 24h, "
        "attesting that electrode SNR>12 dB and calibration drift<2%/hr.",
        "aln/healthcare.continuity.v1.aln::RequiresReliabilityToken",
        "crates/praxis-governance-kernel/src/sensor_integrity.rs::enforce_reliability_token",
        "crates/praxis-governance-kernel/tests/kani_sensor_provenance.rs::kani_prove_reliability_token"
    });
    return spec;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 45. GP psych–electrode coupling demo.
    std::vector<DriftSample> train{
        {0.5, 0.8, 0.02},
        {0.6, 0.9, 0.03},
        {0.4, 0.7, 0.015}
    };

    GPParams gp_params{
        0.2,   // length_scale_p
        0.1,   // length_scale_r
        0.05,  // signal_variance
        0.005  // noise_variance
    };

    DriftSample x_star{0.55, 0.85, 0.0};
    GPPrediction pred = gp_predict(x_star, train, gp_params);

    double K_base = 0.8;
    double sigma_ref = 0.05;
    double K_dyn = dynamic_kalman_gain(K_base, pred.variance, sigma_ref);

    std::cout << "GP-based psych–electrode coupling:\n";
    std::cout << "  Posterior mean drift at (p=0.55,r=0.85): " << pred.mean << "\n";
    std::cout << "  Posterior variance: " << pred.variance << "\n";
    std::cout << "  Base Kalman gain K_base=" << K_base
              << ", dynamic gain K_dyn=" << K_dyn << "\n";
    std::cout << "  Higher GP uncertainty reduces Kalman gain, preventing over-trust\n"
              << "  in psych-state estimates when coupling f is poorly learned.\n\n";

    // 46. DSL crosswalk schema demo.
    StandardSpec spec = make_example_standard();
    print_crosswalk(spec);

    return 0;
}

} // namespace eco
} // namespace praxis
