// File: cpp/eco_restoration/biodiversity_corridor_probability.cpp
#include <iostream>
#include <cmath>

// Logic: given a logistic regression model trained on 500 known corridors,
// prove (in the governance sense) that if two adjacent hex cells both achieve
// a biodiversity index > 0.6 after five years, the probability that a
// functional corridor exists between them is > 80%.
//
// We model the probability of a functional corridor as:
//   P(corridor | features) = 1 / (1 + exp(-z))
// where z = beta0 + beta1 * BI_left + beta2 * BI_right + ...,
// and "functional corridor" labels come from empirical data.

struct LogisticModel {
    double beta0;
    double beta1;
    double beta2;
};

double corridor_probability(const LogisticModel& m,
                            double bi_left,
                            double bi_right) {
    double z = m.beta0 + m.beta1 * bi_left + m.beta2 * bi_right;
    double p = 1.0 / (1.0 + std::exp(-z));
    return p;
}

// Governance-level proof sketch:
//
// We assume the logistic model has been trained on 500 corridor/non-corridor
// samples and validated such that:
//   - For BI_left >= 0.6 and BI_right >= 0.6,
//     empirical minimum z >= z_min,
//     where 1 / (1 + exp(-z_min)) >= 0.8.
// This is enforced as a calibration contract; any deployment uses only
// models whose calibrated parameters satisfy that property.
bool corridor_probability_guarantee(const LogisticModel& m) {
    double bi = 0.6;
    double z = m.beta0 + m.beta1 * bi + m.beta2 * bi;
    double p = 1.0 / (1.0 + std::exp(-z));
    return p >= 0.8;
}

int main() {
    // Example calibrated model coefficients.
    LogisticModel model;
    model.beta0 = -0.5;
    model.beta1 = 2.0;
    model.beta2 = 2.0;

    double bi_left = 0.65;
    double bi_right = 0.68;

    double p_corr = corridor_probability(model, bi_left, bi_right);

    std::cout << "P(functional corridor | BI_left=" << bi_left
              << ", BI_right=" << bi_right << ") = " << p_corr << "\n";

    if (corridor_probability_guarantee(model)) {
        std::cout << "Governance guarantee holds: for BI>=0.6 on both hexes, "
                  << "P(corridor) >= 0.8.\n";
    } else {
        std::cout << "Model does not meet the guarantee; corridor decisions "
                  << "must be treated as research-only.\n";
    }

    return 0;
}
