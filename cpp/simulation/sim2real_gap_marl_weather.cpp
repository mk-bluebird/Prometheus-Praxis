// File: cpp/simulation/sim2real_gap_marl_weather.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>

// Representation of a MARL policy's action response for a given scenario.
// For simplicity, we treat actions as scalar control outputs per time step.
struct ScenarioActionTrace {
    std::string scenario_id;
    std::vector<double> actions_sim;   // actions under simulated environment
    std::vector<double> actions_real;  // actions observed/validated in real environment
    double action_range;               // max |action| range used for normalization
};

struct ScenarioGapResult {
    std::string scenario_id;
    double r_sim2real; // normalized infinity-norm gap
    bool survived;     // true if r_sim2real < threshold
};

class Sim2RealGapQuantifier {
public:
    ScenarioGapResult evaluate(const ScenarioActionTrace& trace,
                               double threshold) const
    {
        if (trace.actions_sim.size() != trace.actions_real.size() ||
            trace.actions_sim.empty()) {
            throw std::runtime_error("ScenarioActionTrace length mismatch or empty.");
        }
        if (trace.action_range <= 0.0) {
            throw std::runtime_error("Invalid action range for normalization.");
        }

        double max_diff = 0.0;
        for (std::size_t i = 0; i < trace.actions_sim.size(); ++i) {
            double diff = std::abs(trace.actions_sim[i] - trace.actions_real[i]);
            if (diff > max_diff) {
                max_diff = diff;
            }
        }

        double r = max_diff / trace.action_range;
        ScenarioGapResult res;
        res.scenario_id = trace.scenario_id;
        res.r_sim2real  = r;
        res.survived    = (r < threshold);
        return res;
    }

    // Aggregate across scenarios: MARL policy is considered domain-robust if all key
    // extreme scenarios satisfy r_sim2real < threshold.
    bool check_domain_performance_ok(const std::vector<ScenarioGapResult>& results,
                                     double threshold,
                                     std::string& failing_scenario) const
    {
        for (const auto& r : results) {
            if (!r.survived || r.r_sim2real >= threshold) {
                failing_scenario = r.scenario_id;
                return false;
            }
        }
        failing_scenario.clear();
        return true;
    }
};

int main() {
    // Define a suite of extreme Phoenix weather scenarios the MARL policy must survive:
    // - "haboob" dust storm events (e.g., PHX-DUST scale 4-5) [279][292]
    // - "extreme_heat_120F" day
    // - "monsoon_humidity_spike"
    // Actions might represent corridor activation levels or cooling intensities per time step.

    std::vector<ScenarioActionTrace> traces;

    // Synthetic traces for illustration; in practice, populated from simulation + field data.
    traces.push_back(ScenarioActionTrace{
        "haboob_scale5",
        {0.3, 0.4, 0.5, 0.6}, // sim
        {0.32,0.38,0.52,0.58}, // real
        1.0
    });
    traces.push_back(ScenarioActionTrace{
        "extreme_heat_120F",
        {0.7, 0.8, 0.9},  // sim
        {0.68,0.82,0.88}, // real
        1.0
    });
    traces.push_back(ScenarioActionTrace{
        "monsoon_humidity_spike",
        {0.5, 0.55, 0.6},  // sim
        {0.52,0.53,0.62},  // real
        1.0
    });

    Sim2RealGapQuantifier quantifier;
    std::vector<ScenarioGapResult> results;
    const double threshold = 0.1; // r_sim2real < 0.1 required

    for (const auto& t : traces) {
        ScenarioGapResult res = quantifier.evaluate(t, threshold);
        std::cout << "Scenario " << res.scenario_id
                  << " r_sim2real=" << res.r_sim2real
                  << " survived=" << (res.survived ? "true" : "false")
                  << "\n";
        results.push_back(res);
    }

    std::string failing;
    bool domain_ok = quantifier.check_domain_performance_ok(results, threshold, failing);

    if (domain_ok) {
        std::cout << "MARL policy survived all extreme Phoenix scenarios with r_sim2real < "
                  << threshold << "; domain_performance_ok can be asserted.\n";
    } else {
        std::cout << "MARL policy failed scenario: " << failing
                  << "; domain_performance_ok must be set to false.\n";
    }

    return 0;
}
