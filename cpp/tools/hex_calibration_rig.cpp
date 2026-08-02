// File: cpp/tools/hex_calibration_rig.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>

// hex_calibration_rig:
// - Command-line tool that reads a CSV of ground-truth temperature measurements per hex cell.
//   CSV format: hex_id, T_measured, T_base, canopy_fraction
// - Fits parameters alpha, beta, gamma in a heat-island formula:
//     T_model = T_base + alpha * (1 - canopy_fraction) + beta * canopy_fraction + gamma
//   via gradient descent minimizing mean squared error between T_model and T_measured.
// - Writes the calibrated parameters to a config file.
//
// This is a self-contained calibration utility for Prometheus-Praxis.

namespace eco {

struct HexSample {
    std::string hex_id;
    double T_measured;
    double T_base;
    double canopy_fraction;
};

class HexCalibrationRig {
public:
    bool load_csv(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "Failed to open CSV: " << path << "\n";
            return false;
        }
        samples_.clear();

        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            HexSample s;
            if (parse_line(line, s)) {
                samples_.push_back(s);
            }
        }

        if (samples_.empty()) {
            std::cerr << "No valid samples found in CSV.\n";
            return false;
        }
        return true;
    }

    void fit_parameters(double alpha_init,
                        double beta_init,
                        double gamma_init,
                        int epochs,
                        double lr) {
        alpha_ = alpha_init;
        beta_  = beta_init;
        gamma_ = gamma_init;

        for (int e = 0; e < epochs; ++e) {
            double d_alpha = 0.0;
            double d_beta  = 0.0;
            double d_gamma = 0.0;
            double loss    = 0.0;

            for (const auto& s : samples_) {
                double t_model = model_temp(s.T_base, s.canopy_fraction);
                double err = t_model - s.T_measured;
                loss += err * err;

                // Derivatives w.r.t parameters.
                double dT_dalpha = (1.0 - s.canopy_fraction);
                double dT_dbeta  = s.canopy_fraction;
                double dT_dgamma = 1.0;

                d_alpha += 2.0 * err * dT_dalpha;
                d_beta  += 2.0 * err * dT_dbeta;
                d_gamma += 2.0 * err * dT_dgamma;
            }

            int n = static_cast<int>(samples_.size());
            loss /= static_cast<double>(n);

            // Gradient descent update.
            alpha_ -= lr * (d_alpha / n);
            beta_  -= lr * (d_beta  / n);
            gamma_ -= lr * (d_gamma / n);

            if (e % (epochs / 10 == 0 ? 1 : epochs / 10) == 0) {
                std::cout << "Epoch " << e
                          << " loss=" << loss
                          << " alpha=" << alpha_
                          << " beta=" << beta_
                          << " gamma=" << gamma_ << "\n";
            }
        }
    }

    bool write_config(const std::string& path) const {
        std::ofstream out(path);
        if (!out) {
            std::cerr << "Failed to open config file for writing: " << path << "\n";
            return false;
        }
        out << std::fixed << std::setprecision(6);
        out << "heat_island_alpha=" << alpha_ << "\n";
        out << "heat_island_beta="  << beta_  << "\n";
        out << "heat_island_gamma=" << gamma_ << "\n";
        return true;
    }

private:
    std::vector<HexSample> samples_;
    double alpha_ = 0.0;
    double beta_  = 0.0;
    double gamma_ = 0.0;

    static bool parse_line(const std::string& line, HexSample& s) {
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> fields;
        while (std::getline(ss, item, ',')) {
            fields.push_back(item);
        }
        if (fields.size() < 4) return false;

        s.hex_id = fields[0];
        try {
            s.T_measured      = std::stod(fields[1]);
            s.T_base          = std::stod(fields[2]);
            s.canopy_fraction = std::stod(fields[3]);
        } catch (...) {
            return false;
        }
        return true;
    }

    double model_temp(double T_base, double canopy_fraction) const {
        return T_base
               + alpha_ * (1.0 - canopy_fraction)
               + beta_  * canopy_fraction
               + gamma_;
    }
};

} // namespace eco

int main(int argc, char** argv) {
    using namespace eco;

    if (argc < 3) {
        std::cerr << "Usage: hex_calibration_rig <input.csv> <output.cfg>\n";
        return 1;
    }

    std::string csv_path = argv[1];
    std::string cfg_path = argv[2];

    HexCalibrationRig rig;
    if (!rig.load_csv(csv_path)) {
        return 1;
    }

    // Initial guesses for alpha, beta, gamma.
    rig.fit_parameters(
        1.0,   // alpha_init
        -0.5,  // beta_init
        0.0,   // gamma_init
        200,   // epochs
        0.0005 // learning rate
    );

    if (!rig.write_config(cfg_path)) {
        return 1;
    }

    std::cout << "Calibration complete. Parameters written to " << cfg_path << "\n";
    return 0;
}
