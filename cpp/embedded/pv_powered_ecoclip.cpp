// File: cpp/embedded/pv_powered_ecoclip.cpp
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <cmath>

// pv_powered_ecoclip:
// - Minimal standalone executable logic intended for an ARM Cortex-M4 deployment
//   (compiled with appropriate cross-toolchain).
// - Runs a subset of eco_cli modes directly on a solar-powered badge:
//   * site assessment (simple soil/water risk scoring)
//   * material scoring (lightweight eco-score as in MaterialInventoryScanner).
// - Uses only fixed-size data structures and basic I/O, suitable for low-power devices.

namespace eco {

struct MaterialProperties {
    const char* material_id;
    const char* name;
    bool recyclable;
    bool biodegradable;
    float embodied_energy_MJ_per_kg;
    float toxicity_index; // 0..1
};

class EcoMaterialScorer {
public:
    EcoMaterialScorer() {
        // Static table of common field materials.
        materials_.push_back({"MAT-PLA",  "Compostable PLA Cup", true,  true,  55.0f, 0.25f});
        materials_.push_back({"MAT-PET",  "PET Bottle",          true,  false, 80.0f, 0.60f});
        materials_.push_back({"MAT-GLASS","Glass Jar",           true,  true,  25.0f, 0.10f});
    }

    const MaterialProperties* lookup(const std::string& id) const {
        for (const auto& m : materials_) {
            if (id == m.material_id) {
                return &m;
            }
        }
        return nullptr;
    }

    float eco_score(const MaterialProperties& m) const {
        float energy_term = 1.0f - std::min(m.embodied_energy_MJ_per_kg / 100.0f, 1.0f);
        float toxicity_term = 1.0f - m.toxicity_index;
        float recyclability_term = m.recyclable ? 1.0f : 0.3f;
        float biodegradability_term = m.biodegradable ? 1.0f : 0.4f;

        float score =
            0.3f * energy_term +
            0.2f * toxicity_term +
            0.25f * recyclability_term +
            0.25f * biodegradability_term;

        if (score < 0.0f) score = 0.0f;
        if (score > 1.0f) score = 1.0f;
        return score;
    }

private:
    std::vector<MaterialProperties> materials_;
};

class SiteAssessment {
public:
    // Simple risk scoring based on soil moisture, slope, and canopy.
    float assess(float soil_moisture_pct,
                 float slope_deg,
                 float canopy_fraction) const {
        // Higher moisture and canopy reduce heat/stress risk; slope affects erosion.
        float moisture_term = std::max(0.0f, 1.0f - soil_moisture_pct / 100.0f);
        float canopy_term = 1.0f - canopy_fraction; // low canopy -> higher risk
        float slope_term = std::min(std::abs(slope_deg) / 30.0f, 1.0f); // >30 deg is high risk

        // Risk index (0..1): higher means more eco-restoration needed.
        float risk = 0.4f * moisture_term + 0.3f * canopy_term + 0.3f * slope_term;
        if (risk < 0.0f) risk = 0.0f;
        if (risk > 1.0f) risk = 1.0f;
        return risk;
    }
};

class PvPoweredEcoClip {
public:
    PvPoweredEcoClip()
        : material_scorer_(), site_assessment_() {}

    void run_demo() {
        std::cout << std::fixed << std::setprecision(2);

        // Material scoring mode (field worker scans a code).
        std::string mat_id = "MAT-PLA";
        const MaterialProperties* m = material_scorer_.lookup(mat_id);
        if (m) {
            float score = material_scorer_.eco_score(*m);
            std::cout << "[EcoClip] Material: " << m->name
                      << " (ID=" << m->material_id << ") eco-score=" << score << "\n";
        } else {
            std::cout << "[EcoClip] Material ID " << mat_id << " not found.\n";
        }

        // Site assessment mode (field worker inputs simple measurements).
        float soil_moisture = 32.0f; // %
        float slope_deg = 8.0f;
        float canopy_frac = 0.18f;

        float risk = site_assessment_.assess(soil_moisture, slope_deg, canopy_frac);
        std::cout << "[EcoClip] Site assessment: soil_moisture=" << soil_moisture
                  << "% slope=" << slope_deg << " deg canopy=" << canopy_frac
                  << " -> risk_index=" << risk << " (0..1)\n";
    }

private:
    EcoMaterialScorer material_scorer_;
    SiteAssessment    site_assessment_;
};

} // namespace eco

int main() {
    using namespace eco;

    PvPoweredEcoClip clip;
    clip.run_demo();

    return 0;
}
