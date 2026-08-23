// Repository: mk-bluebird/Prometheus-Praxis
// Filename: cpp/eco_restoration/DrainageDecay20260822.cpp
// Destination: cpp/eco_restoration/

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct DrainageFrame {
    double hours;
    double bod_mg_l;
    double tss_mg_l;
    double cec_cmol_kg;
    double energyreq_j;
    double delta_vt;
};

struct KerScore {
    double knowledge_factor;
    double eco_impact_value;
    double harm_risk;
};

static void require_range(const std::string& name, double value, double minimum, double maximum) {
    if (!std::isfinite(value) || value < minimum || value > maximum) {
        throw std::invalid_argument(name + " is outside its permitted range");
    }
}

static DrainageFrame project_frame(
    double hours,
    double initial_bod_mg_l,
    double initial_tss_mg_l,
    double initial_cec_cmol_kg,
    double bod_decay_per_hour,
    double tss_decay_per_hour,
    double cec_recovery_per_hour,
    double energyreq_j,
    double delta_vt
) {
    require_range("hours", hours, 0.0, 24.0 * 365.0);
    require_range("initial_bod_mg_l", initial_bod_mg_l, 0.0, 100000.0);
    require_range("initial_tss_mg_l", initial_tss_mg_l, 0.0, 100000.0);
    require_range("initial_cec_cmol_kg", initial_cec_cmol_kg, 0.0, 200.0);
    require_range("bod_decay_per_hour", bod_decay_per_hour, 0.0, 1.0);
    require_range("tss_decay_per_hour", tss_decay_per_hour, 0.0, 1.0);
    require_range("cec_recovery_per_hour", cec_recovery_per_hour, 0.0, 1.0);
    require_range("energyreq_j", energyreq_j, 0.0, 1.0e12);
    require_range("delta_vt", delta_vt, -1000.0, 1000.0);

    const double bod = initial_bod_mg_l * std::exp(-bod_decay_per_hour * hours);
    const double tss = initial_tss_mg_l * std::exp(-tss_decay_per_hour * hours);
    const double cec_capacity = 60.0;
    const double cec = cec_capacity - (cec_capacity - initial_cec_cmol_kg)
        * std::exp(-cec_recovery_per_hour * hours);

    return {hours, bod, tss, cec, energyreq_j, delta_vt};
}

static KerScore score_frame(const DrainageFrame& frame, double sample_completeness) {
    require_range("sample_completeness", sample_completeness, 0.0, 1.0);

    const double bod_quality = std::clamp(1.0 - frame.bod_mg_l / 30.0, 0.0, 1.0);
    const double tss_quality = std::clamp(1.0 - frame.tss_mg_l / 30.0, 0.0, 1.0);
    const double cec_quality = std::clamp(frame.cec_cmol_kg / 30.0, 0.0, 1.0);
    const double energy_quality = std::clamp(1.0 - frame.energyreq_j / 5.0e6, 0.0, 1.0);
    const double voltage_stability = std::clamp(1.0 - std::abs(frame.delta_vt) / 24.0, 0.0, 1.0);

    const double knowledge_factor = std::clamp(
        0.65 * sample_completeness + 0.35 * voltage_stability, 0.0, 1.0
    );
    const double eco_impact_value = std::clamp(
        0.35 * bod_quality + 0.30 * tss_quality + 0.20 * cec_quality + 0.15 * energy_quality,
        0.0, 1.0
    );
    const double harm_risk = std::clamp(
        1.0 - (0.40 * bod_quality + 0.35 * tss_quality + 0.15 * voltage_stability + 0.10 * energy_quality),
        0.0, 1.0
    );

    return {knowledge_factor, eco_impact_value, harm_risk};
}

int main(int argc, char** argv) {
    try {
        if (argc != 10) {
            std::cerr
                << "Usage: " << argv[0]
                << " hours initial_bod_mg_l initial_tss_mg_l initial_cec_cmol_kg"
                << " bod_decay_per_hour tss_decay_per_hour cec_recovery_per_hour"
                << " energyreq_j delta_vt\n";
            return 64;
        }

        const DrainageFrame frame = project_frame(
            std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]),
            std::stod(argv[5]), std::stod(argv[6]), std::stod(argv[7]), std::stod(argv[8]),
            std::stod(argv[9])
        );
        const KerScore ker = score_frame(frame, 1.0);

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "hours=" << frame.hours << "\n";
        std::cout << "bod_mg_l=" << frame.bod_mg_l << "\n";
        std::cout << "tss_mg_l=" << frame.tss_mg_l << "\n";
        std::cout << "cec_cmol_kg=" << frame.cec_cmol_kg << "\n";
        std::cout << "energyreq_j=" << frame.energyreq_j << "\n";
        std::cout << "delta_vt=" << frame.delta_vt << "\n";
        std::cout << "knowledge_factor=" << ker.knowledge_factor << "\n";
        std::cout << "eco_impact_value=" << ker.eco_impact_value << "\n";
        std::cout << "harm_risk=" << ker.harm_risk << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Input error: " << error.what() << "\n";
        return 65;
    }
}
