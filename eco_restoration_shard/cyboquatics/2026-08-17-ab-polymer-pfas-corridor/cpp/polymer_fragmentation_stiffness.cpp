#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct State {
    double coarse_particles;
    double fine_particles;
    double eroded_particles;
};

static double nonnegative(double value) {
    return std::max(0.0, value);
}

static double derivative_total(double total_particles, double k_hyd, double k_frag, double alpha) {
    if (total_particles <= 0.0) {
        return 0.0;
    }
    return k_hyd * total_particles - k_frag * std::pow(total_particles, alpha);
}

static State advance_rk4(
    State state,
    double step_days,
    double k_hyd,
    double k_frag,
    double erosion_per_day,
    double alpha
) {
    const auto slope = [=](const State& value) {
        const double fragmentation = k_frag * std::pow(nonnegative(value.coarse_particles), alpha);
        const double hydrolysis = k_hyd * nonnegative(value.coarse_particles);
        const double erosion = erosion_per_day * nonnegative(value.fine_particles);
        return State{
            -(fragmentation + hydrolysis),
            fragmentation + hydrolysis - erosion,
            erosion
        };
    };

    const State k1 = slope(state);
    const State s2{
        state.coarse_particles + 0.5 * step_days * k1.coarse_particles,
        state.fine_particles + 0.5 * step_days * k1.fine_particles,
        state.eroded_particles + 0.5 * step_days * k1.eroded_particles
    };
    const State k2 = slope(s2);
    const State s3{
        state.coarse_particles + 0.5 * step_days * k2.coarse_particles,
        state.fine_particles + 0.5 * step_days * k2.fine_particles,
        state.eroded_particles + 0.5 * step_days * k2.eroded_particles
    };
    const State k3 = slope(s3);
    const State s4{
        state.coarse_particles + step_days * k3.coarse_particles,
        state.fine_particles + step_days * k3.fine_particles,
        state.eroded_particles + step_days * k3.eroded_particles
    };
    const State k4 = slope(s4);

    return State{
        nonnegative(state.coarse_particles + step_days * (k1.coarse_particles + 2.0 * k2.coarse_particles + 2.0 * k3.coarse_particles + k4.coarse_particles) / 6.0),
        nonnegative(state.fine_particles + step_days * (k1.fine_particles + 2.0 * k2.fine_particles + 2.0 * k3.fine_particles + k4.fine_particles) / 6.0),
        nonnegative(state.eroded_particles + step_days * (k1.eroded_particles + 2.0 * k2.eroded_particles + 2.0 * k3.eroded_particles + k4.eroded_particles) / 6.0)
    };
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr
            << "usage: " << argv[0]
            << " <k_hyd_per_day> <k_frag_per_day> <erosion_per_day> <alpha>"
            << " <initial_coarse_particles> <step_days> <steps>\n";
        return 64;
    }

    try {
        const double k_hyd = std::stod(argv[1]);
        const double k_frag = std::stod(argv[2]);
        const double erosion = std::stod(argv[3]);
        const double alpha = std::stod(argv[4]);
        const double initial_coarse = std::stod(argv[5]);
        const double step_days = std::stod(argv[6]);
        const int steps = std::stoi(argv[7]);

        if (k_hyd < 0.0 || k_frag < 0.0 || erosion < 0.0 || alpha <= 0.0 ||
            initial_coarse < 0.0 || step_days <= 0.0 || steps <= 0) {
            throw std::invalid_argument("rates and initial state must be non-negative; alpha, step, and steps must be positive");
        }

        const double initial_total = initial_coarse;
        const double stiffness_proxy = std::max(
            k_hyd + k_frag * alpha * std::pow(std::max(1.0, initial_total), std::max(0.0, alpha - 1.0)),
            erosion
        ) * step_days;

        State state{initial_coarse, 0.0, 0.0};
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "time_days,coarse_particles,fine_particles,eroded_particles,total_derivative\n";

        for (int step = 0; step <= steps; ++step) {
            const double time_days = step * step_days;
            const double retained = state.coarse_particles + state.fine_particles;
            const double total_derivative = derivative_total(retained, k_hyd, k_frag, alpha);
            std::cout << time_days << ','
                      << state.coarse_particles << ','
                      << state.fine_particles << ','
                      << state.eroded_particles << ','
                      << total_derivative << '\n';

            if (step < steps) {
                state = advance_rk4(state, step_days, k_hyd, k_frag, erosion, alpha);
            }
        }

        const double observability_threshold = std::max(1.0, initial_coarse * 0.05);
        const bool bimodal_screen = state.coarse_particles >= observability_threshold &&
                                    state.fine_particles >= observability_threshold;

        std::cout << "stiffness_proxy=" << stiffness_proxy << '\n';
        std::cout << "integration_guidance=" << (stiffness_proxy > 0.20 ? "REDUCE_STEP_OR_USE_IMPLICIT_SOLVER" : "EXPLICIT_RK4_ACCEPTABLE_FOR_SCREENING") << '\n';
        std::cout << "bimodal_screen=" << (bimodal_screen ? "SIZE_RESOLVED_TWO_MODE_POSSIBLE" : "NOT_DEMONSTRATED") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
