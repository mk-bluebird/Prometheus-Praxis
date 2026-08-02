// File: cpp/tools/eco_digital_health_score.cpp
#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <iomanip>
#include <cmath>
#include <limits>

namespace eco {

// Self-reported or inferred digital eco habits for one household.
struct HouseholdSurveyResponse {
    double eco_focus_score;             // 0..1, share of digital activity on eco-positive content
    double risky_prompt_fraction;       // 0..1, fraction of prompts/queries deemed risky or non-eco
    double avg_dwell_eco_minutes;       // average dwell time on eco pages per session (minutes)
    double perceived_eco_digital_health; // 0..1, survey label: household's own eco-digital health rating
    std::string corridor_id;           // smart-city corridor / hex identifier (e.g., PHX-HEX-001)
};

// Combined eco-digital health score formula:
// DHS = w1*(eco_focus_score)
//     + w2*(1 - risky_prompt_fraction)
//     + w3*(avg_dwell_on_eco_pages / 10.0)
//
// Weights w1, w2, w3 are learned from a survey of Phoenix households.
struct EcoDigitalHealthWeights {
    double w1;
    double w2;
    double w3;
};

// Clamp helper.
static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

// Compute DHS for one household, normalized and bounded.
double compute_dhs(const EcoDigitalHealthWeights& w,
                   double eco_focus_score,
                   double risky_prompt_fraction,
                   double avg_dwell_eco_minutes) {
    double eco_term  = clamp01(eco_focus_score);
    double safe_term = clamp01(1.0 - risky_prompt_fraction);
    double dwell_term = avg_dwell_eco_minutes / 10.0;
    if (dwell_term < 0.0) dwell_term = 0.0;
    // dwell_term can exceed 1.0 if dwell >>10min; leave that as-is so very
    // high eco dwell is recognized, but we will clamp final DHS.

    double dhs = w.w1 * eco_term
               + w.w2 * safe_term
               + w.w3 * dwell_term;

    // Bound DHS to a sensible range [0, 1.5] then rescale to [0,1] for corridor use.
    if (dhs < 0.0) dhs = 0.0;
    if (dhs > 1.5) dhs = 1.5;
    return dhs / 1.5;
}

// Fit weights with a simple least-squares regression on survey labels.
// Model: perceived_eco_digital_health ≈ w1*x1 + w2*x2 + w3*x3,
// where:
//   x1 = eco_focus_score,
//   x2 = 1 - risky_prompt_fraction,
//   x3 = avg_dwell_eco_minutes / 10.
//
// This uses normal equations for 3 parameters and guards against degenerate fits.
EcoDigitalHealthWeights fit_weights_from_survey(const std::vector<HouseholdSurveyResponse>& responses) {
    EcoDigitalHealthWeights w{0.0, 0.0, 0.0};

    if (responses.empty()) {
        w.w1 = w.w2 = w.w3 = 1.0 / 3.0;
        return w;
    }

    double a11 = 0.0, a12 = 0.0, a13 = 0.0;
    double a22 = 0.0, a23 = 0.0, a33 = 0.0;
    double b1  = 0.0, b2  = 0.0, b3  = 0.0;

    for (const auto& r : responses) {
        double x1 = clamp01(r.eco_focus_score);
        double x2 = clamp01(1.0 - r.risky_prompt_fraction);
        double x3 = r.avg_dwell_eco_minutes / 10.0;
        if (x3 < 0.0) x3 = 0.0;

        double y = clamp01(r.perceived_eco_digital_health);

        a11 += x1 * x1;
        a12 += x1 * x2;
        a13 += x1 * x3;
        a22 += x2 * x2;
        a23 += x2 * x3;
        a33 += x3 * x3;

        b1  += x1 * y;
        b2  += x2 * y;
        b3  += x3 * y;
    }

    double det =
        a11 * (a22 * a33 - a23 * a23)
      - a12 * (a12 * a33 - a13 * a23)
      + a13 * (a12 * a23 - a13 * a22);

    if (std::fabs(det) < 1e-12) {
        // Degenerate system; fall back to equal weights.
        w.w1 = w.w2 = w.w3 = 1.0 / 3.0;
        return w;
    }

    double det1 =
        b1 * (a22 * a33 - a23 * a23)
      - a12 * (b2 * a33 - a23 * b3)
      + a13 * (b2 * a23 - a22 * b3);

    double det2 =
        a11 * (b2 * a33 - a23 * b3)
      - b1 * (a12 * a33 - a13 * a23)
      + a13 * (a12 * b3 - b2 * a13);

    double det3 =
        a11 * (a22 * b3 - b2 * a23)
      - a12 * (a12 * b3 - b2 * a13)
      + b1 * (a12 * a23 - a13 * a22);

    w.w1 = det1 / det;
    w.w2 = det2 / det;
    w.w3 = det3 / det;

    return w;
}

// Smart-city aggregation: compute corridor-level DHS averages and export as a simple text endpoint.
struct CorridorDHS {
    std::string corridor_id;
    double mean_dhs;
};

std::vector<CorridorDHS> aggregate_by_corridor(const EcoDigitalHealthWeights& w,
                                               const std::vector<HouseholdSurveyResponse>& responses) {
    std::unordered_map<std::string, std::pair<double, int>> corridor_acc;
    for (const auto& r : responses) {
        double dhs = compute_dhs(w, r.eco_focus_score,
                                 r.risky_prompt_fraction,
                                 r.avg_dwell_eco_minutes);
        auto& acc = corridor_acc[r.corridor_id];
        acc.first += dhs;
        acc.second += 1;
    }

    std::vector<CorridorDHS> out;
    out.reserve(corridor_acc.size());
    for (const auto& kv : corridor_acc) {
        const std::string& id = kv.first;
        double sum = kv.second.first;
        int count = kv.second.second;
        double mean = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
        out.push_back(CorridorDHS{id, mean});
    }
    return out;
}

} // namespace eco

int main() {
    using namespace eco;

    // Example survey data from Phoenix households (synthetic demo).
    std::vector<HouseholdSurveyResponse> survey;
    survey.push_back(HouseholdSurveyResponse{0.7, 0.1, 12.0, 0.85, "PHX-HEX-001"});
    survey.push_back(HouseholdSurveyResponse{0.5, 0.2,  8.0, 0.70, "PHX-HEX-001"});
    survey.push_back(HouseholdSurveyResponse{0.9, 0.05, 15.0, 0.95, "PHX-HEX-002"});
    survey.push_back(HouseholdSurveyResponse{0.3, 0.4,  4.0, 0.50, "PHX-HEX-003"});
    survey.push_back(HouseholdSurveyResponse{0.6, 0.15, 10.0, 0.78, "PHX-HEX-002"});

    EcoDigitalHealthWeights w = fit_weights_from_survey(survey);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Learned weights (Phoenix smart-city):\n";
    std::cout << "  w1 (eco_focus_score)        = " << w.w1 << "\n";
    std::cout << "  w2 (1 - risky_prompt_frac)  = " << w.w2 << "\n";
    std::cout << "  w3 (dwell/10min)            = " << w.w3 << "\n\n";

    double eco_focus_score        = 0.8;
    double risky_prompt_fraction  = 0.12;
    double avg_dwell_eco_minutes  = 11.0;

    double dhs = compute_dhs(w, eco_focus_score,
                             risky_prompt_fraction,
                             avg_dwell_eco_minutes);

    std::cout << "Household eco-digital health score (DHS, normalized 0..1) = " << dhs << "\n\n";

    // Corridor-level aggregation for smart-city dashboards / KER envelopes.
    auto corridor_scores = aggregate_by_corridor(w, survey);
    std::cout << "Corridor DHS aggregates:\n";
    for (const auto& c : corridor_scores) {
        std::cout << "  " << c.corridor_id << " mean_DHS=" << c.mean_dhs << "\n";
    }

    return 0;
}
