// File: cpp/simulation/eligibility_surrogate_explainability.cpp

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

// Simple feature vector for DecideStatus inputs (SevenDimProfile + key evidence flags).
struct EligibilityFeatures {
    double knowledge_factor;
    double robustness;
    double eco_impact;
    double sovereignty_alignment;
    double energy_efficiency;
    double community_alignment;
    double explainability;
    double risk_residual;
    bool   safety_case_documented;
    bool   sovereignty_compliant;
    bool   energy_neutral_or_renew;
};

// Ground-truth eligibility label from DecideStatus (e.g., "Eligible", "Pilot", "NotEligible").
struct EligibilitySample {
    EligibilityFeatures x;
    std::string status;
};

// Minimal decision-tree surrogate: a set of human-readable threshold rules.
struct SurrogateRule {
    std::string description;
    std::string predicted_status;
};

struct SurrogateModel {
    std::vector<SurrogateRule> rules;
};

// Confusion counts for fidelity scoring.
struct ConfusionCounts {
    int tp;
    int fp;
    int fn;
    int tn;
};

class EligibilitySurrogateTrainer {
public:
    // For simplicity, we build a small, human-readable surrogate manually from thresholds
    // that approximate DecideStatus behavior. In practice, this would be trained from data.
    SurrogateModel build_surrogate() const {
        SurrogateModel model;
        // Rule 1: Eligible when high eco-impact, robustness, and low risk.
        model.rules.push_back(SurrogateRule{
            "IF eco_impact >= 0.8 AND robustness >= 0.8 AND risk_residual <= 0.3 "
            "AND safety_case_documented AND sovereignty_compliant AND energy_neutral_or_renew "
            "THEN status = Eligible",
            "Eligible"
        });
        // Rule 2: Pilot when eco-impact moderate but safety case documented.
        model.rules.push_back(SurrogateRule{
            "IF eco_impact >= 0.6 AND robustness >= 0.6 AND risk_residual <= 0.5 "
            "AND safety_case_documented THEN status = Pilot",
            "Pilot"
        });
        // Rule 3: NotEligible otherwise.
        model.rules.push_back(SurrogateRule{
            "ELSE status = NotEligible",
            "NotEligible"
        });
        return model;
    }

    // Apply surrogate model to a single feature vector.
    std::string predict(const SurrogateModel& model, const EligibilityFeatures& x) const {
        // Rule 1
        if (x.eco_impact >= 0.8 &&
            x.robustness >= 0.8 &&
            x.risk_residual <= 0.3 &&
            x.safety_case_documented &&
            x.sovereignty_compliant &&
            x.energy_neutral_or_renew) {
            return "Eligible";
        }
        // Rule 2
        if (x.eco_impact >= 0.6 &&
            x.robustness >= 0.6 &&
            x.risk_residual <= 0.5 &&
            x.safety_case_documented) {
            return "Pilot";
        }
        // Rule 3
        return "NotEligible";
    }

    // Compute F1-score treating "Eligible" as the positive class.
    double compute_f1(const SurrogateModel& model,
                      const std::vector<EligibilitySample>& samples) const
    {
        ConfusionCounts cc{0, 0, 0, 0};
        for (const auto& s : samples) {
            std::string y_true = s.status;
            std::string y_pred = predict(model, s.x);
            bool positive_true = (y_true == "Eligible");
            bool positive_pred = (y_pred == "Eligible");

            if (positive_true && positive_pred) cc.tp++;
            else if (!positive_true && positive_pred) cc.fp++;
            else if (positive_true && !positive_pred) cc.fn++;
            else cc.tn++;
        }
        double precision = (cc.tp + cc.fp > 0) ? static_cast<double>(cc.tp) / (cc.tp + cc.fp) : 0.0;
        double recall    = (cc.tp + cc.fn > 0) ? static_cast<double>(cc.tp) / (cc.tp + cc.fn) : 0.0;
        if (precision + recall == 0.0) return 0.0;
        double f1 = 2.0 * precision * recall / (precision + recall);
        return f1;
    }
};

int main() {
    // Synthetic training/evaluation set derived from true DecideStatus outputs.
    std::vector<EligibilitySample> samples = {
        // Eligible
        {{0.9,0.9,0.85,0.9,0.88,0.9,0.9,0.25,true,true,true},"Eligible"},
        {{0.92,0.87,0.82,0.91,0.86,0.88,0.89,0.28,true,true,true},"Eligible"},
        // Pilot
        {{0.7,0.7,0.65,0.8,0.75,0.8,0.8,0.40,true,true,true},"Pilot"},
        {{0.65,0.68,0.62,0.78,0.72,0.77,0.79,0.45,true,true,true},"Pilot"},
        // NotEligible
        {{0.5,0.5,0.4,0.6,0.5,0.5,0.5,0.60,false,true,true},"NotEligible"},
        {{0.6,0.55,0.45,0.7,0.6,0.6,0.6,0.55,true,false,true},"NotEligible"},
        {{0.7,0.7,0.6,0.8,0.75,0.8,0.8,0.50,true,true,false},"NotEligible"}
    };

    EligibilitySurrogateTrainer trainer;
    SurrogateModel model = trainer.build_surrogate();
    double f1 = trainer.compute_f1(model, samples);

    std::cout << "Surrogate decision-tree F1-score (Eligible vs non-Eligible) = " << f1 << "\n";

    // Governance requirement: minimum fidelity for explainable_and_audited.
    // For example, F1 >= 0.9 is required to set explainable_and_audited = true.
    double min_fidelity = 0.9;
    bool explainable_and_audited = (f1 >= min_fidelity);

    std::cout << "explainable_and_audited: " << (explainable_and_audited ? "true" : "false") << "\n";

    // Exposure in ResearchSnapshot: print human-readable rules for Phoenix community auditors.
    std::cout << "\nHuman-readable surrogate rules:\n";
    for (const auto& r : model.rules) {
        std::cout << "- " << r.description << "\n";
    }

    return 0;
}
