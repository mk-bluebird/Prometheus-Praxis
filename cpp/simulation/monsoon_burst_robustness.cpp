// File: cpp/simulation/monsoon_burst_robustness.cpp

#include <iostream>
#include <cmath>
#include <stdexcept>
#include <string>

struct BurstConfig {
    double lambda_burst;     // burst arrival rate [events per second]
    double t_buffer;         // buffer residence horizon [seconds]
    double target_r_burst;   // maximum acceptable overflow probability (e.g., 0.05)
};

struct BufferDesign {
    std::size_t buffer_depth_events; // number of events buffer can hold
    double compute_rate_events_per_s; // sustained processing rate
    double r_burst;                   // resulting overflow probability for design horizon
};

class MonsoonBurstRobustnessDesigner {
public:
    // We use the risk coordinate definition:
    // r_burst = 1 - exp(-lambda_burst * T_buffer)
    // and interpret T_buffer as (buffer_depth / effective_net_arrival_rate).
    // For a given lambda_burst and compute_rate, we choose buffer_depth so that
    // r_burst < target_r_burst, then compute robustness adjustments.
    BufferDesign design(double lambda_burst,
                        double compute_rate_events_per_s,
                        double target_r_burst) const
    {
        if (lambda_burst <= 0.0 || compute_rate_events_per_s <= 0.0) {
            throw std::runtime_error("Invalid burst or compute rate.");
        }
        if (target_r_burst <= 0.0 || target_r_burst >= 1.0) {
            throw std::runtime_error("Invalid target r_burst.");
        }

        // Effective net arrival relative to compute:
        // We require compute_rate >= lambda_burst for long-term stability;
        // during a burst, excess arrivals accumulate in the buffer.
        if (compute_rate_events_per_s < lambda_burst) {
            throw std::runtime_error("Compute rate must exceed burst arrival rate for robustness.");
        }

        // Solve for T_buffer such that r_burst = 1 - exp(-lambda_burst * T_buffer) <= target_r_burst:
        // exp(-lambda_burst * T_buffer) >= 1 - target_r_burst
        // -lambda_burst * T_buffer >= ln(1 - target_r_burst)
        // T_buffer <= -ln(1 - target_r_burst) / lambda_burst
        double t_buffer_max = -std::log(1.0 - target_r_burst) / lambda_burst;

        // Translate T_buffer_max into buffer depth:
        // During worst-case 1-in-10-year storm, net backlog per second is (lambda_burst - compute_rate).
        // To keep overflow probability below target, buffer_depth >= max_backlog = (lambda_burst - compute_rate) * T_buffer_max.
        double net_backlog_rate = lambda_burst - compute_rate_events_per_s;
        if (net_backlog_rate < 0.0) {
            net_backlog_rate = 0.0; // compute can keep up; minimal buffer depth suffices.
        }

        double required_depth = net_backlog_rate * t_buffer_max;
        std::size_t buffer_depth_events = static_cast<std::size_t>(std::ceil(required_depth));

        // Compute resulting r_burst for chosen T_buffer = t_buffer_max
        double r_burst = 1.0 - std::exp(-lambda_burst * t_buffer_max);

        BufferDesign design;
        design.buffer_depth_events = buffer_depth_events;
        design.compute_rate_events_per_s = compute_rate_events_per_s;
        design.r_burst = r_burst;
        return design;
    }

    // Adjust robustness dimension based on r_burst: lower overflow probability yields higher robustness.
    double robustness_from_r_burst(double base_robustness, double r_burst) const {
        // Simple mapping: robustness = base_robustness * (1 - r_burst / r_burst_ref)
        // with r_burst_ref = 0.2 as a reference "poor robustness" level.
        double r_burst_ref = 0.2;
        double factor = 1.0 - std::min(1.0, r_burst / r_burst_ref);
        double robustness = base_robustness * std::max(0.0, factor);
        return robustness;
    }
};

int main() {
    // Example: 1-in-10-year monsoon burst characterised by high lightning/dust sensor rate.
    // Suppose lambda_burst ~ 50 events/s during peak burst (Poisson rate).
    // Target r_burst < 0.05.
    double lambda_burst = 50.0;          // events per second
    double compute_rate = 80.0;          // pipeline can process 80 events/s under burst provisioning
    double target_r_burst = 0.05;        // overflow probability ceiling

    MonsoonBurstRobustnessDesigner designer;
    BufferDesign bd = designer.design(lambda_burst, compute_rate, target_r_burst);

    std::cout << "Monsoon burst buffer design:\n";
    std::cout << "lambda_burst = " << lambda_burst << " events/s\n";
    std::cout << "compute_rate = " << bd.compute_rate_events_per_s << " events/s\n";
    std::cout << "buffer_depth_events = " << bd.buffer_depth_events << "\n";
    std::cout << "r_burst (overflow probability) = " << bd.r_burst << "\n";

    double base_robustness = 0.85; // baseline robustness from other factors
    double robustness = designer.robustness_from_r_burst(base_robustness, bd.r_burst);
    std::cout << "Adjusted Robustness dimension under burst: " << robustness << "\n";

    return 0;
}
