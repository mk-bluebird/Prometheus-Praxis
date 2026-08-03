// File: cpp/eco_restoration/causal_impact_estimator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>

namespace eco {

// Historical eco-telemetry for a single hex over time.
struct HexTelemetry {
    std::string hex_id;
    std::vector<double> outcome;   // e.g. ker_s or heat-stress index per period
    std::vector<bool> treated;     // true if green infrastructure active in period
};

// Causal impact estimate for a single hex.
struct HexEcoROI {
    std::string hex_id;
    double did_effect;      // difference-in-differences estimate
    double synthetic_effect; // synthetic control-style estimate (simple weighted average)
    double roi;             // ROI_h = Δ(outcome) / Δ(delta_v_t), here proxy using did_effect
};

// Compute mean outcome over periods satisfying a predicate.
double mean_outcome(const HexTelemetry& telem, bool treated_flag, bool post_treatment_flag) {
    double sum = 0.0;
    int count = 0;
    std::size_t T = telem.outcome.size();
    for (std::size_t t = 0; t < T; ++t) {
        bool is_post = (t >= T / 2); // simple split: first half pre, second half post
        if (telem.treated[t] == treated_flag && is_post == post_treatment_flag) {
            sum += telem.outcome[t];
            ++count;
        }
    }
    if (count == 0) return 0.0;
    return sum / static_cast<double>(count);
}

// Difference-in-differences: (treated_post - treated_pre) - (control_post - control_pre)
double did_estimate(const HexTelemetry& treated_hex,
                    const HexTelemetry& control_hex) {
    double treated_pre = mean_outcome(treated_hex, true, false);
    double treated_post = mean_outcome(treated_hex, true, true);
    double control_pre = mean_outcome(control_hex, false, false);
    double control_post = mean_outcome(control_hex, false, true);

    double treated_diff = treated_post - treated_pre;
    double control_diff = control_post - control_pre;
    return treated_diff - control_diff;
}

// Simple synthetic control: match pre-treatment average to compute weight, then predict post.
double synthetic_control_estimate(const HexTelemetry& treated_hex,
                                  const std::vector<HexTelemetry>& donors) {
    double treated_pre = mean_outcome(treated_hex, true, false);

    // Choose donor with closest pre-treatment mean as synthetic control.
    double best_diff = std::numeric_limits<double>::infinity();
    const HexTelemetry* best_donor = nullptr;
    for (const auto& d : donors) {
        double donor_pre = mean_outcome(d, false, false);
        double diff = std::fabs(donor_pre - treated_pre);
        if (diff < best_diff) {
            best_diff = diff;
            best_donor = &d;
        }
    }
    if (!best_donor) return 0.0;

    double treated_post = mean_outcome(treated_hex, true, true);
    double donor_post = mean_outcome(*best_donor, false, true);
    return treated_post - donor_post;
}

// Compute ROI record for a hex given treated and control data.
// For simplicity, ROI_h is proportional to DiD effect.
HexEcoROI compute_hex_roi(const HexTelemetry& treated_hex,
                          const HexTelemetry& control_hex,
                          const std::vector<HexTelemetry>& donors) {
    double did = did_estimate(treated_hex, control_hex);
    double synth = synthetic_control_estimate(treated_hex, donors);

    HexEcoROI roi{};
    roi.hex_id = treated_hex.hex_id;
    roi.did_effect = did;
    roi.synthetic_effect = synth;
    roi.roi = did; // stand-in ROI metric

    return roi;
}

void print_hex_roi_sql(const HexEcoROI& roi) {
    std::cout << "INSERT INTO hex_eco_roi_history "
              << "(hex_id, did_effect, synthetic_effect, roi_metric) VALUES ('"
              << roi.hex_id << "', "
              << roi.did_effect << ", "
              << roi.synthetic_effect << ", "
              << roi.roi << ");\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example telemetry: treated hex with green infrastructure, control hex without.
    HexTelemetry treated{"hex_GI_001", {}, {}};
    HexTelemetry control{"hex_CTRL_001", {}, {}};

    // Simulated 12 periods: first 6 pre, last 6 post.
    for (int t = 0; t < 12; ++t) {
        double base_outcome = 0.5 - 0.02 * t;       // e.g. decreasing heat-stress index
        double gi_bonus = (t >= 6) ? -0.05 : 0.0;   // green infra improves outcome in post
        treated.outcome.push_back(base_outcome + gi_bonus);
        treated.treated.push_back(true);

        control.outcome.push_back(base_outcome);
        control.treated.push_back(false);
    }

    // Donor pool for synthetic control (other untreated hexes).
    std::vector<HexTelemetry> donors;
    HexTelemetry donor1{"hex_DONOR_001", {}, {}};
    HexTelemetry donor2{"hex_DONOR_002", {}, {}};
    for (int t = 0; t < 12; ++t) {
        donor1.outcome.push_back(0.5 - 0.018 * t);
        donor1.treated.push_back(false);
        donor2.outcome.push_back(0.52 - 0.021 * t);
        donor2.treated.push_back(false);
    }
    donors.push_back(donor1);
    donors.push_back(donor2);

    HexEcoROI roi = compute_hex_roi(treated, control, donors);
    print_hex_roi_sql(roi);

    return 0;
}
