// File: cpp/eco_restoration/multitask_bod_pfas_seq2seq_physics.cpp

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <string>
#include <stdexcept>
#include <fstream>
#include <algorithm>

// This file implements a multi-task seq2seq-style forecaster for joint BOD and PFAS,
// with a custom physics-informed loss based on a discrete advection-dispersion term.
// It uses a simple GRU-like recurrent cell in pure C++ instead of DL4J, but exposes
// model parameters and training artifacts that can be consumed by Java/Kotlin or DL4J
// via serialized weight files. The physics loss penalizes violations of
// C_{t+1} - C_t - dt*(-v * dC/dx + D * d2C/dx2) = 0 in discrete time for both BOD and PFAS.

namespace prometheus_praxis {
namespace eco_restoration {

struct TelemetrySample {
    // Minimal telemetry variables: flow velocity, dispersion, upstream/downstream BOD/PFAS.
    double flow_velocity;   // v (m/s)
    double dispersion_coeff; // D (m^2/s)
    double bod_upstream;    // BOD at upstream sensor
    double bod_downstream;  // BOD at downstream sensor
    double pfas_upstream;   // PFAS at upstream sensor
    double pfas_downstream; // PFAS at downstream sensor
};

struct ForecastTarget {
    double bod_next;
    double pfas_next;
};

struct Seq2SeqTrainingExample {
    std::vector<TelemetrySample> input_seq;
    ForecastTarget target;
};

struct PhysicsParams {
    double dt;     // time step
    double dx;     // spatial separation between upstream and downstream
    double lambda; // physics penalty weight
};

struct GRUParams {
    std::size_t input_dim;
    std::size_t hidden_dim;
    std::size_t output_dim;
};

struct GRUWeights {
    // GRU-style single-layer sequence encoder.
    // Wz, Wr, Wh: input weights; Uz, Ur, Uh: recurrent weights; bz, br, bh: biases.
    std::vector<double> Wz;
    std::vector<double> Uz;
    std::vector<double> bz;

    std::vector<double> Wr;
    std::vector<double> Ur;
    std::vector<double> br;

    std::vector<double> Wh;
    std::vector<double> Uh;
    std::vector<double> bh;

    // Output projection to 2 tasks: BOD and PFAS.
    std::vector<double> Wy; // hidden_dim x output_dim
    std::vector<double> by; // output_dim
};

class GRUSeq2SeqModel {
public:
    GRUSeq2SeqModel(const GRUParams& params, const PhysicsParams& phys)
        : params_(params), phys_(phys) {
        initWeights();
    }

    // Forward pass: encode input sequence, output BOD/PFAS forecast.
    ForecastTarget forward(const std::vector<TelemetrySample>& seq) const {
        std::vector<double> h(params_.hidden_dim, 0.0);
        for (const auto& s : seq) {
            std::vector<double> x = telemetryToInputVector(s);
            stepGRU(x, h);
        }
        ForecastTarget out;
        out.bod_next = 0.0;
        out.pfas_next = 0.0;
        for (std::size_t j = 0; j < params_.output_dim; ++j) {
            double sum = by_[j];
            for (std::size_t i = 0; i < params_.hidden_dim; ++i) {
                sum += Wy_[i * params_.output_dim + j] * h[i];
            }
            if (j == 0) out.bod_next = sum;
            else out.pfas_next = sum;
        }
        return out;
    }

    // Custom loss: L = MSE(pred, target) + lambda * physics_penalty.
    double loss(const Seq2SeqTrainingExample& ex,
                const ForecastTarget& pred) const {
        double mse_bod = (pred.bod_next - ex.target.bod_next) * (pred.bod_next - ex.target.bod_next);
        double mse_pfas = (pred.pfas_next - ex.target.pfas_next) * (pred.pfas_next - ex.target.pfas_next);
        double mse = 0.5 * (mse_bod + mse_pfas);

        double phys_loss = physicsPenalty(ex, pred);
        return mse + phys_.lambda * phys_loss;
    }

    // Stochastic gradient descent training with teacher forcing curriculum.
    // In this simple implementation, we treat gradients as finite differences for demonstration.
    void train(std::vector<Seq2SeqTrainingExample>& dataset,
               std::size_t n_epochs,
               double lr) {
        if (dataset.empty()) return;
        std::mt19937 rng(42);

        for (std::size_t epoch = 0; epoch < n_epochs; ++epoch) {
            std::shuffle(dataset.begin(), dataset.end(), rng);
            double epoch_loss = 0.0;
            std::size_t count = 0;

            // Teacher forcing curriculum: early epochs use ground truth in physics penalty,
            // later epochs gradually incorporate model predictions in the physics term.
            double teacher_ratio = 1.0 - static_cast<double>(epoch) / static_cast<double>(n_epochs);
            teacher_ratio = std::max(0.0, teacher_ratio);

            for (auto& ex : dataset) {
                ForecastTarget pred = forward(ex.input_seq);
                double L = lossWithTeacher(ex, pred, teacher_ratio);
                epoch_loss += L;
                count++;

                // Simple parameter update via finite-difference approximation on output projection only.
                // In practice, this should be replaced by backpropagation; here we keep it lightweight.
                std::vector<double> grad_Wy(Wy_.size(), 0.0);
                std::vector<double> grad_by(by_.size(), 0.0);

                computeOutputGradients(ex, pred, grad_Wy, grad_by);

                for (std::size_t k = 0; k < Wy_.size(); ++k) {
                    Wy_[k] -= lr * grad_Wy[k];
                }
                for (std::size_t k = 0; k < by_.size(); ++k) {
                    by_[k] -= lr * grad_by[k];
                }
            }

            epoch_loss /= static_cast<double>(count);
            std::cout << "Epoch " << epoch << " avg loss=" << epoch_loss
                      << " teacher_ratio=" << teacher_ratio << std::endl;
        }
    }

    // Serialize weights to a simple text file that Kotlin/Java/DL4J can read.
    void saveWeights(const std::string& path) const {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("Failed to open weight file for writing");
        }
        out << params_.input_dim << " " << params_.hidden_dim << " " << params_.output_dim << "\n";

        auto save_vec = [&](const std::vector<double>& v) {
            for (double x : v) out << x << " ";
            out << "\n";
        };

        save_vec(Wz_);
        save_vec(Uz_);
        save_vec(bz_);
        save_vec(Wr_);
        save_vec(Ur_);
        save_vec(br_);
        save_vec(Wh_);
        save_vec(Uh_);
        save_vec(bh_);
        save_vec(Wy_);
        save_vec(by_);
    }

private:
    GRUParams params_;
    PhysicsParams phys_;

    // GRU weights and output projection.
    std::vector<double> Wz_;
    std::vector<double> Uz_;
    std::vector<double> bz_;

    std::vector<double> Wr_;
    std::vector<double> Ur_;
    std::vector<double> br_;

    std::vector<double> Wh_;
    std::vector<double> Uh_;
    std::vector<double> bh_;

    std::vector<double> Wy_;
    std::vector<double> by_;

    void initWeights() {
        std::mt19937 rng(1);
        std::normal_distribution<double> dist(0.0, 0.1);

        auto init_vec = [&](std::size_t n) {
            std::vector<double> v(n);
            for (std::size_t i = 0; i < n; ++i) v[i] = dist(rng);
            return v;
        };

        Wz_ = init_vec(params_.input_dim * params_.hidden_dim);
        Uz_ = init_vec(params_.hidden_dim * params_.hidden_dim);
        bz_ = init_vec(params_.hidden_dim);

        Wr_ = init_vec(params_.input_dim * params_.hidden_dim);
        Ur_ = init_vec(params_.hidden_dim * params_.hidden_dim);
        br_ = init_vec(params_.hidden_dim);

        Wh_ = init_vec(params_.input_dim * params_.hidden_dim);
        Uh_ = init_vec(params_.hidden_dim * params_.hidden_dim);
        bh_ = init_vec(params_.hidden_dim);

        Wy_ = init_vec(params_.hidden_dim * params_.output_dim);
        by_ = init_vec(params_.output_dim);
    }

    static double sigmoid(double x) {
        return 1.0 / (1.0 + std::exp(-x));
    }

    static double tanh_act(double x) {
        return std::tanh(x);
    }

    std::vector<double> telemetryToInputVector(const TelemetrySample& s) const {
        std::vector<double> x(params_.input_dim, 0.0);
        // Input ordering: [v, D, bod_up, bod_down, pfas_up, pfas_down]
        x[0] = s.flow_velocity;
        x[1] = s.dispersion_coeff;
        x[2] = s.bod_upstream;
        x[3] = s.bod_downstream;
        x[4] = s.pfas_upstream;
        x[5] = s.pfas_downstream;
        return x;
    }

    void stepGRU(const std::vector<double>& x, std::vector<double>& h) const {
        std::vector<double> z(params_.hidden_dim, 0.0);
        std::vector<double> r(params_.hidden_dim, 0.0);
        std::vector<double> h_tilde(params_.hidden_dim, 0.0);

        for (std::size_t i = 0; i < params_.hidden_dim; ++i) {
            double z_i = bz_[i];
            double r_i = br_[i];
            double h_i = bh_[i];
            for (std::size_t j = 0; j < params_.input_dim; ++j) {
                z_i += Wz_[j * params_.hidden_dim + i] * x[j];
                r_i += Wr_[j * params_.hidden_dim + i] * x[j];
                h_i += Wh_[j * params_.hidden_dim + i] * x[j];
            }
            for (std::size_t j = 0; j < params_.hidden_dim; ++j) {
                z_i += Uz_[j * params_.hidden_dim + i] * h[j];
                r_i += Ur_[j * params_.hidden_dim + i] * h[j];
            }
            z[i] = sigmoid(z_i);
            r[i] = sigmoid(r_i);

            double h_rec = 0.0;
            for (std::size_t j = 0; j < params_.hidden_dim; ++j) {
                h_rec += Uh_[j * params_.hidden_dim + i] * (r[i] * h[j]);
            }
            h_tilde[i] = tanh_act(h_i + h_rec);
        }

        for (std::size_t i = 0; i < params_.hidden_dim; ++i) {
            h[i] = (1.0 - z[i]) * h[i] + z[i] * h_tilde[i];
        }
    }

    // Discrete advection-dispersion physics penalty for final time step.
    // For BOD and PFAS, we compute residual of:
    // C_down_next - C_down_curr - dt*(-v * (C_down_curr - C_up_curr)/dx + D * (C_up_curr - 2*C_down_curr + C_down_curr)/dx^2) = 0
    // We approximate second derivative with a simple two-point stencil; for demonstration.
    double physicsPenalty(const Seq2SeqTrainingExample& ex,
                          const ForecastTarget& pred) const {
        if (ex.input_seq.empty()) return 0.0;
        const TelemetrySample& last = ex.input_seq.back();
        double v = last.flow_velocity;
        double D = last.dispersion_coeff;

        double bod_up = last.bod_upstream;
        double bod_down = last.bod_downstream;
        double pfas_up = last.pfas_upstream;
        double pfas_down = last.pfas_downstream;

        double bod_next = pred.bod_next;
        double pfas_next = pred.pfas_next;

        double dCdx_bod = (bod_down - bod_up) / phys_.dx;
        double dCdx_pfas = (pfas_down - pfas_up) / phys_.dx;

        double d2Cdx2_bod = (bod_up - 2.0 * bod_down + bod_down) / (phys_.dx * phys_.dx);
        double d2Cdx2_pfas = (pfas_up - 2.0 * pfas_down + pfas_down) / (phys_.dx * phys_.dx);

        double physics_res_bod = bod_next - bod_down - phys_.dt * (-v * dCdx_bod + D * d2Cdx2_bod);
        double physics_res_pfas = pfas_next - pfas_down - phys_.dt * (-v * dCdx_pfas + D * d2Cdx2_pfas);

        return 0.5 * (physics_res_bod * physics_res_bod + physics_res_pfas * physics_res_pfas);
    }

    // Teacher-forcing variant of loss: mix true next values into physics penalty
    // with probability teacher_ratio.
    double lossWithTeacher(const Seq2SeqTrainingExample& ex,
                           const ForecastTarget& pred,
                           double teacher_ratio) const {
        ForecastTarget phys_pred = pred;
        if (!ex.input_seq.empty() && teacher_ratio > 0.0) {
            // Mix target into physics term.
            phys_pred.bod_next = teacher_ratio * ex.target.bod_next + (1.0 - teacher_ratio) * pred.bod_next;
            phys_pred.pfas_next = teacher_ratio * ex.target.pfas_next + (1.0 - teacher_ratio) * pred.pfas_next;
        }
        double mse_bod = (pred.bod_next - ex.target.bod_next) * (pred.bod_next - ex.target.bod_next);
        double mse_pfas = (pred.pfas_next - ex.target.pfas_next) * (pred.pfas_next - ex.target.pfas_next);
        double mse = 0.5 * (mse_bod + mse_pfas);
        double phys_loss = physicsPenalty(ex, phys_pred);
        return mse + phys_.lambda * phys_loss;
    }

    // Approximate gradients of output projection via finite differences (for demonstration).
    void computeOutputGradients(const Seq2SeqTrainingExample& ex,
                                const ForecastTarget& pred,
                                std::vector<double>& grad_Wy,
                                std::vector<double>& grad_by) const {
        double eps = 1e-4;
        ForecastTarget base_pred = pred;
        double base_loss = lossWithTeacher(ex, base_pred, 0.0);

        // Gradient w.r.t. by (bias).
        for (std::size_t j = 0; j < params_.output_dim; ++j) {
            ForecastTarget pert_pred = pred;
            if (j == 0) pert_pred.bod_next += eps;
            else pert_pred.pfas_next += eps;
            double L_plus = lossWithTeacher(ex, pert_pred, 0.0);
            grad_by[j] = (L_plus - base_loss) / eps;
        }

        // Assume hidden state is fixed; gradient w.r.t Wy elements approximated via output perturbation.
        // In practice, we'd need hidden state; we approximate using unit hidden dimension perturbation.
        for (std::size_t i = 0; i < params_.hidden_dim; ++i) {
            for (std::size_t j = 0; j < params_.output_dim; ++j) {
                std::size_t idx = i * params_.output_dim + j;
                grad_Wy[idx] = 0.0; // simplified; real backprop would be used in DL4J side.
            }
        }
    }
};

// Simple sliding-window dataset generation from continuous telemetry.
std::vector<Seq2SeqTrainingExample> buildSlidingWindowDataset(
    const std::vector<TelemetrySample>& telemetry,
    std::size_t window_len) {

    std::vector<Seq2SeqTrainingExample> dataset;
    if (telemetry.size() <= window_len) return dataset;

    for (std::size_t i = 0; i + window_len < telemetry.size(); ++i) {
        Seq2SeqTrainingExample ex;
        ex.input_seq.assign(telemetry.begin() + i, telemetry.begin() + i + window_len);
        const TelemetrySample& next = telemetry[i + window_len];
        ex.target.bod_next = next.bod_downstream;
        ex.target.pfas_next = next.pfas_downstream;
        dataset.push_back(ex);
    }
    return dataset;
}

// Simple persistence baseline: predict next value as current downstream value.
double baselineMSEPersistence(const std::vector<Seq2SeqTrainingExample>& dataset) {
    if (dataset.empty()) return 0.0;
    double accum = 0.0;
    std::size_t count = 0;
    for (const auto& ex : dataset) {
        if (ex.input_seq.empty()) continue;
        const TelemetrySample& last = ex.input_seq.back();
        double pred_bod = last.bod_downstream;
        double pred_pfas = last.pfas_downstream;
        double mse_bod = (pred_bod - ex.target.bod_next) * (pred_bod - ex.target.bod_next);
        double mse_pfas = (pred_pfas - ex.target.pfas_next) * (pred_pfas - ex.target.pfas_next);
        accum += 0.5 * (mse_bod + mse_pfas);
        count++;
    }
    return count > 0 ? accum / static_cast<double>(count) : 0.0;
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main() {
    using namespace prometheus_praxis::eco_restoration;

    // Synthetic dataset demonstrating BOD/PFAS joint forecasting with physics-informed loss.
    std::vector<TelemetrySample> telemetry;
    std::mt19937 rng(123);
    std::normal_distribution<double> noise(0.0, 0.1);

    double v = 0.2;
    double D = 0.05;
    double bod_up = 5.0;
    double pfas_up = 0.5;
    double bod_down = 4.5;
    double pfas_down = 0.4;

    for (std::size_t t = 0; t < 200; ++t) {
        TelemetrySample s;
        s.flow_velocity = v;
        s.dispersion_coeff = D;
        s.bod_upstream = bod_up;
        s.bod_downstream = bod_down;
        s.pfas_upstream = pfas_up;
        s.pfas_downstream = pfas_down;
        telemetry.push_back(s);

        // Simple synthetic dynamics: decay with slight advection downstream.
        bod_up = bod_up * std::exp(-0.01) + noise(rng);
        pfas_up = pfas_up * std::exp(-0.005) + noise(rng) * 0.1;
        bod_down = bod_up - 0.1 + noise(rng);
        pfas_down = pfas_up - 0.05 + noise(rng) * 0.05;
    }

    std::size_t window_len = 8;
    auto dataset = buildSlidingWindowDataset(telemetry, window_len);

    GRUParams gru_params;
    gru_params.input_dim = 6;
    gru_params.hidden_dim = 16;
    gru_params.output_dim = 2;

    PhysicsParams phys_params;
    phys_params.dt = 1.0;
    phys_params.dx = 10.0;
    phys_params.lambda = 0.5; // physics penalty weight

    GRUSeq2SeqModel model(gru_params, phys_params);

    double baseline_mse = baselineMSEPersistence(dataset);
    std::cout << "Baseline persistence MSE (BOD/PFAS): " << baseline_mse << std::endl;

    model.train(dataset, 10, 0.001);

    // Evaluate trained model on dataset.
    double mse_model = 0.0;
    std::size_t count = 0;
    for (const auto& ex : dataset) {
        ForecastTarget pred = model.forward(ex.input_seq);
        double mse_bod = (pred.bod_next - ex.target.bod_next) * (pred.bod_next - ex.target.bod_next);
        double mse_pfas = (pred.pfas_next - ex.target.pfas_next) * (pred.pfas_next - ex.target.pfas_next);
        mse_model += 0.5 * (mse_bod + mse_pfas);
        count++;
    }
    mse_model /= static_cast<double>(count);
    std::cout << "Model MSE (BOD/PFAS) after physics-informed training: " << mse_model << std::endl;

    // Serialize weights for DL4J/Kotlin service consumption.
    model.saveWeights("multitask_bod_pfas_seq2seq_weights.txt");
    std::cout << "Weights saved to multitask_bod_pfas_seq2seq_weights.txt" << std::endl;

    return 0;
}
