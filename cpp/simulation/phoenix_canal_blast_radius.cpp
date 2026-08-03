// File: cpp/simulation/phoenix_canal_blast_radius.cpp
#include <vector>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace phoenix_canal {

struct GridPoint {
    double x;          // meters
    double y;          // meters
    double soil_diffusivity;   // m^2/s
    double drain_adv_x;        // m/s
    double drain_adv_y;        // m/s
    double topology_decay;     // 1/s, lambda(x; tau)
};

struct BlastState {
    std::vector<double> energy;      // E(x,y,t) at each grid point
};

struct BlastParams {
    double dt_s;              // time step [s]
    double dx_m;              // spatial step in x [m]
    double dy_m;              // spatial step in y [m]
    double source_energy;     // initial surcharge energy at canal node
};

struct BlastRisk {
    double r_hydraulics;      // normalized hydraulic blast radius risk [0,1]
    double r_topology;        // normalized topology sensitivity risk [0,1]
    double r_energy;          // normalized energy load risk [0,1]
};

class BlastRadiusModel {
public:
    BlastRadiusModel(const std::vector<GridPoint>& grid,
                     const BlastParams& params,
                     double hydraulic_threshold,
                     double topology_threshold,
                     double energy_threshold)
        : grid_(grid),
          params_(params),
          hydraulic_threshold_(hydraulic_threshold),
          topology_threshold_(topology_threshold),
          energy_threshold_(energy_threshold) {
        if (grid_.empty()) {
            throw std::invalid_argument("Grid must not be empty");
        }
        if (params_.dt_s <= 0.0 || params_.dx_m <= 0.0 || params_.dy_m <= 0.0) {
            throw std::invalid_argument("Time and space steps must be positive");
        }
        state_.energy.assign(grid_.size(), 0.0);
        // Inject source energy at index 0 (canal node anchor).
        state_.energy[0] = params_.source_energy;
    }

    void step() {
        std::vector<double> next_energy(state_.energy.size(), 0.0);

        for (std::size_t i = 0; i < grid_.size(); ++i) {
            const GridPoint& gp = grid_[i];
            double E = state_.energy[i];

            double D = gp.soil_diffusivity;
            double ux = gp.drain_adv_x;
            double uy = gp.drain_adv_y;
            double lambda = gp.topology_decay;

            // Simple finite-difference diffusion–advection–reaction update.
            double diff_term = D * laplacian(i);
            double adv_term  = - (ux * gradient_x(i) + uy * gradient_y(i));
            double react_term = - lambda * E;

            double dE_dt = diff_term + adv_term + react_term;
            double E_next = E + params_.dt_s * dE_dt;
            if (E_next < 0.0) E_next = 0.0;

            next_energy[i] = E_next;
        }

        state_.energy.swap(next_energy);
    }

    BlastRisk compute_risk() const {
        double max_energy = 0.0;
        double radius_energy_sum = 0.0;
        double topology_weighted_energy = 0.0;

        for (std::size_t i = 0; i < grid_.size(); ++i) {
            const GridPoint& gp = grid_[i];
            double E = state_.energy[i];

            if (E > max_energy) max_energy = E;

            double r = std::sqrt(gp.x * gp.x + gp.y * gp.y);
            radius_energy_sum += r * E;
            topology_weighted_energy += gp.topology_decay * E;
        }

        double r_energy = max_energy / energy_threshold_;
        if (r_energy > 1.0) r_energy = 1.0;
        if (r_energy < 0.0) r_energy = 0.0;

        double avg_radius_energy = radius_energy_sum / (params_.source_energy + 1e-9);
        double r_hydraulics = avg_radius_energy / hydraulic_threshold_;
        if (r_hydraulics > 1.0) r_hydraulics = 1.0;
        if (r_hydraulics < 0.0) r_hydraulics = 0.0;

        double avg_topology_energy = topology_weighted_energy / (params_.source_energy + 1e-9);
        double r_topology = avg_topology_energy / topology_threshold_;
        if (r_topology > 1.0) r_topology = 1.0;
        if (r_topology < 0.0) r_topology = 0.0;

        BlastRisk risk{};
        risk.r_hydraulics = r_hydraulics;
        risk.r_topology   = r_topology;
        risk.r_energy     = r_energy;
        return risk;
    }

    const BlastState& state() const {
        return state_;
    }

private:
    double laplacian(std::size_t idx) const {
        double E_center = state_.energy[idx];
        double E_left   = (idx > 0) ? state_.energy[idx - 1] : E_center;
        double E_right  = (idx + 1 < state_.energy.size()) ? state_.energy[idx + 1] : E_center;

        double dx2 = params_.dx_m * params_.dx_m;
        double d2E_dx2 = (E_left - 2.0 * E_center + E_right) / dx2;
        return d2E_dx2;
    }

    double gradient_x(std::size_t idx) const {
        double E_center = state_.energy[idx];
        double E_right  = (idx + 1 < state_.energy.size()) ? state_.energy[idx + 1] : E_center;
        return (E_right - E_center) / params_.dx_m;
    }

    double gradient_y(std::size_t idx) const {
        // In this simplified 1D radial model, we treat y-gradient as zero.
        return 0.0;
    }

    std::vector<GridPoint> grid_;
    BlastParams params_;
    double hydraulic_threshold_;
    double topology_threshold_;
    double energy_threshold_;
    BlastState state_;
};

} // namespace phoenix_canal

int main() {
    using namespace phoenix_canal;

    std::vector<GridPoint> grid = {
        {0.0, 0.0, 0.01, 0.0, 0.0, 0.05},
        {10.0, 0.0, 0.01, 0.0, 0.0, 0.04},
        {20.0, 0.0, 0.01, 0.0, 0.0, 0.03},
        {30.0, 0.0, 0.01, 0.0, 0.0, 0.02}
    };

    BlastParams params{1.0, 10.0, 10.0, 1000.0};
    BlastRadiusModel model(grid, params,
                           /*hydraulic_threshold=*/20.0,
                           /*topology_threshold=*/0.5,
                           /*energy_threshold=*/1000.0);

    for (int t = 0; t < 60; ++t) {
        model.step();
        BlastRisk risk = model.compute_risk();
        std::cout << "t=" << t
                  << " r_hydraulics=" << risk.r_hydraulics
                  << " r_topology=" << risk.r_topology
                  << " r_energy=" << risk.r_energy
                  << "\n";
    }

    return 0;
}
