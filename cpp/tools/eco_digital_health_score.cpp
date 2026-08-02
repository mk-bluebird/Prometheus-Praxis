// File: cpp/tools/eco_digital_health_score.cpp
#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <iomanip>
#include <cmath>

namespace eco {

struct HouseholdSurveyResponse {
    // Self-reported or inferred digital eco habits for one household.
    double eco_focus_score;          // 0..1, share of digital activity on eco-positive content
    double risky_prompt_fraction;    // 0..1, fraction of prompts/queries deemed risky or non-eco
    double avg_dwell_eco_minutes;    // average dwell time on eco pages per session (minutes)
    double perceived_eco_digital_health; // 0..1, survey label: household's own eco-digital health rating
};

// Combined eco-digital health score formula:
// DHS = w1*(eco_focus_score)
//     + w2*(1 - risky_prompt_fraction)
//     + w3*(avg_dwell_on_eco_pages / 10.0)
//
// Weights w1, w2, w3 are learned from a survey of 200 Phoenix households.
struct EcoDigitalHealthWeights {
    double w1;
    double w2;
    double w3;
};

double compute_dhs(const EcoDigitalHealthWeights& w,
                   double eco_focus_score,
                   double risky_prompt_fraction,
                   double avg_dwell_eco_minutes) {
    double safe_prompt_term = 1.0 - risky_prompt_fraction;
    if (safe_prompt_term < 0.0) safe_prompt_term = 0.0;
    if (safe_prompt_term > 1.0) safe_prompt_term = 1.0;

    double dwell_term = avg_dwell_eco_minutes / 10.0;
    if (dwell_term < 0.0) dwell_term = 0.0;

    double dhs = w.w1 * eco_focus_score
               + w.w2 * safe_prompt_term
               + w.w3 * dwell_term;
    return dhs;
}

// Fit weights with a simple least-squares regression on survey labels.
// Model: perceived_eco_digital_health ≈ w1*x1 + w2*x2 + w3*x3,
// where:
//   x1 = eco_focus_score,
//   x2 = 1 - risky_prompt_fraction,
//   x3 = avg_dwell_eco_minutes / 10.
//
// This uses normal equations for 3 parameters.
EcoDigitalHealthWeights fit_weights_from_survey(const std::vector<HouseholdSurveyResponse>& responses) {
    // Design matrix components: A^T A (3x3) and A^T y (3x1)
    double a11 = 0.0, a12 = 0.0, a13 = 0.0;
    double a22 = 0.0, a23 = 0.0, a33 = 0.0;
    double b1  = 0.0, b2  = 0.0, b3  = 0.0;

    for (const auto& r : responses) {
        double x1 = r.eco_focus_score;
        double x2 = 1.0 - r.risky_prompt_fraction;
        if (x2 < 0.0) x2 = 0.0;
        if (x2 > 1.0) x2 = 1.0;
        double x3 = r.avg_dwell_eco_minutes / 10.0;
        if (x3 < 0.0) x3 = 0.0;

        double y = r.perceived_eco_digital_health;

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

    // Symmetric matrix:
    // [a11 a12 a13]
    // [a12 a22 a23]
    // [a13 a23 a33]
    //
    // Solve (A^T A) w = A^T y using Cramer's rule or direct inversion.

    // Compute determinant.
    double det =
        a11 * (a22 * a33 - a23 * a23)
      - a12 * (a12 * a33 - a13 * a23)
      + a13 * (a12 * a23 - a13 * a22);

    EcoDigitalHealthWeights w{0.0, 0.0, 0.0};

    if (std::fabs(det) < 1e-12) {
        // Degenerate system; fall back to equal weights.
        w.w1 = w.w2 = w.w3 = 1.0 / 3.0;
        return w;
    }

    // Inverse (A^T A)^(-1) times b: compute via adjugate/det.
    // w1 numerator (determinant of matrix with b column in first place).
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

} // namespace eco

int main() {
    using namespace eco;

    // Example survey data from Phoenix households (synthetic demo).
    std::vector<HouseholdSurveyResponse> survey;
    survey.push_back(HouseholdSurveyResponse{0.7, 0.1, 12.0, 0.85});
    survey.push_back(HouseholdSurveyResponse{0.5, 0.2, 8.0, 0.70});
    survey.push_back(HouseholdSurveyResponse{0.9, 0.05, 15.0, 0.95});
    survey.push_back(HouseholdSurveyResponse{0.3, 0.4, 4.0, 0.50});
    survey.push_back(HouseholdSurveyResponse{0.6, 0.15, 10.0, 0.78});

    EcoDigitalHealthWeights w = fit_weights_from_survey(survey);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Learned weights:\n";
    std::cout << "  w1 (eco_focus_score)        = " << w.w1 << "\n";
    std::cout << "  w2 (1 - risky_prompt_frac)  = " << w.w2 << "\n";
    std::cout << "  w3 (dwell/10min)            = " << w.w3 << "\n\n";

    // Compute DHS for a sample household.
    double eco_focus_score        = 0.8;
    double risky_prompt_fraction  = 0.12;
    double avg_dwell_eco_minutes  = 11.0;

    double dhs = compute_dhs(w, eco_focus_score,
                             risky_prompt_fraction,
                             avg_dwell_eco_minutes);

    std::cout << "Combined eco-digital health score (DHS) = " << dhs << "\n";

    return 0;
}
