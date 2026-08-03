// File: cpp/simulation/gan_governance_stress_tester.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

namespace eco {

struct SyntheticWorkload {
    std::string id;
    std::string hex_id;
    double k;
    double e;
    double r;
    double eco_energy;
    double eco_efficiency;
    double carbon_intensity;
    double max_carbon;
};

struct GovernanceParams {
    double alpha;
    double beta;
    double gamma;
    double delta;
    double delta_V_max;
    double s_min_non_research;
    double c_min;
};

struct StressTestRecord {
    std::string workload_id;
    std::string hex_id;
    double ker_s;
    double carbon_corridor;
    double delta_V;
    bool ker_violation;
    bool delta_V_violation;
    bool carbon_violation;
};

double ker_scalar(double k, double e, double r, double s_min_non_research) {
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    double s = k * e - r;
    if (s < s_min_non_research) {
        s = s_min_non_research;
    }
    return s;
}

double carbon_corridor(double ci, double max_carbon, double c_min) {
    if (max_carbon <= 0.0) return 0.0;
    double c = 1.0 - ci / max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    if (c < c_min) c = c_min;
    return c;
}

double eco_energy_term(double eco_energy, double eco_eff, double alpha) {
    if (eco_eff < 0.0) eco_eff = 0.0;
    if (eco_eff > 1.0) eco_eff = 1.0;
    return eco_energy * (1.0 + alpha * (1.0 - eco_eff));
}

// Full corridor trigger: ΔV_t <= min(ΔV_max, γ s, δ c)
StressTestRecord run_triggers(const SyntheticWorkload& w,
                              const GovernanceParams& gp) {
    StressTestRecord rec{};
    rec.workload_id = w.id;
    rec.hex_id = w.hex_id;

    rec.ker_s = ker_scalar(w.k, w.e, w.r, gp.s_min_non_research);
    rec.carbon_corridor = carbon_corridor(w.carbon_intensity, w.max_carbon, gp.c_min);
    double E_eco = eco_energy_term(w.eco_energy, w.eco_efficiency, gp.alpha);
    rec.delta_V = gp.beta * E_eco;

    double bound_ker = gp.gamma * rec.ker_s;
    double bound_carbon = gp.delta * rec.carbon_corridor;
    double upper = gp.delta_V_max;
    if (bound_ker < upper) upper = bound_ker;
    if (bound_carbon < upper) upper = bound_carbon;

    rec.ker_violation = (rec.ker_s <= 0.0);
    rec.carbon_violation = (rec.carbon_corridor <= 0.0);
    rec.delta_V_violation = (rec.delta_V > upper);

    return rec;
}

// Harness: push synthetic workloads through KER/ΔV_t/carbon pipeline and log violations.
std::vector<StressTestRecord> run_stress_test(
        const std::vector<SyntheticWorkload>& workloads,
        const GovernanceParams& gp) {
    std::vector<StressTestRecord> records;
    records.reserve(workloads.size());
    for (const auto& w : workloads) {
        StressTestRecord rec = run_triggers(w, gp);
        records.push_back(rec);
    }
    return records;
}

// Emit SQL logging violations.
void emit_violation_sql(const std::vector<StressTestRecord>& records,
                        const std::string& run_id) {
    for (const auto& rec : records) {
        if (!(rec.ker_violation || rec.delta_V_violation || rec.carbon_violation)) {
            continue;
        }
        std::cout << "INSERT INTO governance_stress_violations "
                  << "(run_id, workload_id, hex_id, ker_s, carbon_corridor, delta_v_t, "
                  << "ker_violation, delta_v_violation, carbon_violation) VALUES ('"
                  << run_id << "', '"
                  << rec.workload_id << "', '"
                  << rec.hex_id << "', "
                  << rec.ker_s << ", "
                  << rec.carbon_corridor << ", "
                  << rec.delta_V << ", "
                  << (rec.ker_violation ? 1 : 0) << ", "
                  << (rec.delta_V_violation ? 1 : 0) << ", "
                  << (rec.carbon_violation ? 1 : 0) << ");\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    GovernanceParams gp{};
    gp.alpha = 0.5;
    gp.beta = 0.01;
    gp.gamma = 0.1;
    gp.delta = 0.1;
    gp.delta_V_max = 0.05;
    gp.s_min_non_research = 0.05;
    gp.c_min = 0.2;

    // Example synthetic workloads (would be loaded from GAN-exported JSON/CSV).
    std::vector<SyntheticWorkload> workloads = {
        {"gan_w1", "hex_GAN_1", 0.9, 0.85, 0.2, 1.2, 0.9, 0.4, 1.0},
        {"gan_w2", "hex_GAN_2", 0.7, 0.6, 0.5, 1.5, 0.7, 0.8, 1.0},
        {"gan_w3", "hex_GAN_3", 0.5, 0.5, 0.6, 2.0, 0.6, 0.9, 1.0}
    };

    auto records = run_stress_test(workloads, gp);
    std::string run_id = "gan_stress_2026_08_03";

    std::cout << "GAN-based governance stress test results:\n";
    for (const auto& rec : records) {
        std::cout << "  " << rec.workload_id << " on " << rec.hex_id
                  << " : s=" << rec.ker_s
                  << " c=" << rec.carbon_corridor
                  << " ΔV=" << rec.delta_V
                  << " ker_violation=" << (rec.ker_violation ? "YES" : "NO")
                  << " delta_V_violation=" << (rec.delta_V_violation ? "YES" : "NO")
                  << " carbon_violation=" << (rec.carbon_violation ? "YES" : "NO")
                  << "\n";
    }
    std::cout << "\n";

    emit_violation_sql(records, run_id);

    return 0;
}
