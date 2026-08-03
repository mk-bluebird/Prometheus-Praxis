// File: cpp/simulation/hex_stability_anomaly_detector.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>

namespace eco {

struct HexSample {
    std::string hex_id;
    // ΔV_t samples over a diagnostic window (e.g. last N steps)
    std::vector<double> delta_V_samples;
};

struct AnomalyRecord {
    std::string hex_id;
    double eigenvalue;     // anomalous eigenvalue
    double lambda_minus;   // Marchenko–Pastur lower bound
    double lambda_plus;    // Marchenko–Pastur upper bound
    bool is_anomalous;
};

class HexStabilityAnomalyDetector {
public:
    // Build covariance matrix of ΔV_t samples across hexes
    static std::vector<std::vector<double>> covariance_matrix(
            const std::vector<HexSample>& samples) {
        std::size_t H = samples.size();
        if (H == 0) return {};
        std::size_t N = samples[0].delta_V_samples.size();
        if (N == 0) return {};

        // Mean per hex
        std::vector<double> mean(H, 0.0);
        for (std::size_t h = 0; h < H; ++h) {
            for (double v : samples[h].delta_V_samples) {
                mean[h] += v;
            }
            mean[h] /= static_cast<double>(samples[h].delta_V_samples.size());
        }

        // Covariance C_{ij} = (1/N) sum_t (x_i(t)-m_i)(x_j(t)-m_j)
        std::vector<std::vector<double>> C(H, std::vector<double>(H, 0.0));
        for (std::size_t i = 0; i < H; ++i) {
            for (std::size_t j = i; j < H; ++j) {
                double sum = 0.0;
                for (std::size_t t = 0; t < N; ++t) {
                    double xi = samples[i].delta_V_samples[t] - mean[i];
                    double xj = samples[j].delta_V_samples[t] - mean[j];
                    sum += xi * xj;
                }
                double val = sum / static_cast<double>(N);
                C[i][j] = val;
                C[j][i] = val;
            }
        }
        return C;
    }

    // Simple power iteration to approximate largest eigenvalue of covariance matrix
    static double largest_eigenvalue(const std::vector<std::vector<double>>& C,
                                     int max_iter = 1000,
                                     double tol = 1e-6) {
        std::size_t H = C.size();
        if (H == 0) return 0.0;
        std::vector<double> v(H, 1.0 / std::sqrt(static_cast<double>(H)));

        double lambda_old = 0.0;
        for (int it = 0; it < max_iter; ++it) {
            // w = C v
            std::vector<double> w(H, 0.0);
            for (std::size_t i = 0; i < H; ++i) {
                for (std::size_t j = 0; j < H; ++j) {
                    w[i] += C[i][j] * v[j];
                }
            }
            // normalize w
            double norm = 0.0;
            for (double x : w) norm += x * x;
            norm = std::sqrt(norm);
            if (norm < 1e-12) break;
            for (std::size_t i = 0; i < H; ++i) {
                v[i] = w[i] / norm;
            }
            // Rayleigh quotient v^T C v
            double lambda = 0.0;
            for (std::size_t i = 0; i < H; ++i) {
                double Ci_v = 0.0;
                for (std::size_t j = 0; j < H; ++j) {
                    Ci_v += C[i][j] * v[j];
                }
                lambda += v[i] * Ci_v;
            }
            if (std::fabs(lambda - lambda_old) < tol) {
                return lambda;
            }
            lambda_old = lambda;
        }
        return lambda_old;
    }

    // Marchenko–Pastur interval [λ-, λ+] for correlation matrices with σ^2=1, λ=m/n
    static void marchenko_pastur_bounds(std::size_t m, std::size_t n,
                                        double& lambda_minus,
                                        double& lambda_plus) {
        if (n == 0) {
            lambda_minus = 0.0;
            lambda_plus = 0.0;
            return;
        }
        double ratio = static_cast<double>(m) / static_cast<double>(n);
        double root = std::sqrt(ratio);
        lambda_minus = std::pow(1.0 - root, 2);
        lambda_plus = std::pow(1.0 + root, 2);
    }

    static AnomalyRecord detect_anomaly(const std::vector<HexSample>& samples) {
        std::size_t H = samples.size();
        if (H == 0) {
            return {"", 0.0, 0.0, 0.0, false};
        }
        std::size_t N = samples[0].delta_V_samples.size();
        auto C = covariance_matrix(samples);
        double lambda_max = largest_eigenvalue(C);

        double lambda_minus = 0.0;
        double lambda_plus = 0.0;
        marchenko_pastur_bounds(H, N, lambda_minus, lambda_plus);

        bool anomalous = (lambda_max > lambda_plus || lambda_max < lambda_minus);

        // For simplicity, we flag global anomaly; mapping back to hex IDs can be refined
        return { "GLOBAL",
                 lambda_max,
                 lambda_minus,
                 lambda_plus,
                 anomalous };
    }

    static void print_record(const AnomalyRecord& rec) {
        std::cout << "Hex stability anomaly record:\n";
        std::cout << "  scope_id: " << rec.hex_id << "\n";
        std::cout << "  largest_eigenvalue: " << rec.eigenvalue << "\n";
        std::cout << "  MP_lambda_minus: " << rec.lambda_minus << "\n";
        std::cout << "  MP_lambda_plus: " << rec.lambda_plus << "\n";
        std::cout << "  anomalous: " << (rec.is_anomalous ? "true" : "false") << "\n";
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Example ΔV_t samples for three hexes over 20 time steps
    std::vector<HexSample> samples;
    HexSample h1{"hex_A", {}};
    HexSample h2{"hex_B", {}};
    HexSample h3{"hex_C", {}};

    for (int t = 0; t < 20; ++t) {
        h1.delta_V_samples.push_back(0.01 + 0.001 * t);
        h2.delta_V_samples.push_back(0.015 + 0.0005 * t);
        h3.delta_V_samples.push_back(0.012 + 0.002 * t);
    }
    samples.push_back(h1);
    samples.push_back(h2);
    samples.push_back(h3);

    AnomalyRecord rec = HexStabilityAnomalyDetector::detect_anomaly(samples);
    HexStabilityAnomalyDetector::print_record(rec);

    return 0;
}
