// File: cpp/simulation/multi_hex_habitat_planner.cpp
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <fstream>
#include <iomanip>

// multi_hex_habitat_planner:
// - Simulates hundreds of hex cells with synthetic Phoenix land-use data.
// - Runs a simple habitat_regeneration_simulator on each hex over time.
// - Writes the combined biodiversity time series to a NetCDF-like ASCII file.
//
// Real NetCDF requires external libraries; here, we use a structured text format
// that is easy to convert to NetCDF in a separate step, preserving eco-simulation logic.

namespace eco {

struct HexCell {
    std::string hex_id;
    double initial_canopy_fraction;
    double initial_biodiversity_index; // 0..1
    double urban_intensity;            // 0..1 (higher = more built-up)
};

struct BiodiversityTimePoint {
    double time_years;
    double biodiversity_index;         // 0..1
};

class HabitatRegenerationSimulator {
public:
    // Simple regeneration model:
    // dB/dt = r * (1 - urban_intensity) * (canopy_fraction + 0.1) * (1 - B) - d * urban_intensity * B
    HabitatRegenerationSimulator(double r, double d)
        : r_(r), d_(d) {}

    std::vector<BiodiversityTimePoint> simulate(const HexCell& cell,
                                                double years,
                                                double dt_years) const {
        std::vector<BiodiversityTimePoint> series;
        double B = cell.initial_biodiversity_index;
        double canopy = cell.initial_canopy_fraction;
        double urban = cell.urban_intensity;

        int steps = static_cast<int>(years / dt_years);
        for (int i = 0; i <= steps; ++i) {
            double t = i * dt_years;
            series.push_back(BiodiversityTimePoint{t, clamp01(B)});

            double regen = r_ * (1.0 - urban) * (canopy + 0.1) * (1.0 - B);
            double decay = d_ * urban * B;
            double dBdt = regen - decay;

            B += dBdt * dt_years;
        }
        return series;
    }

private:
    double r_;
    double d_;

    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

class MultiHexHabitatPlanner {
public:
    MultiHexHabitatPlanner(int num_hex)
        : num_hex_(num_hex), rng_(123) {
        generate_hex_cells();
    }

    void run_and_write(const std::string& output_path,
                       double years,
                       double dt_years) {
        HabitatRegenerationSimulator sim(0.4, 0.15);

        std::ofstream out(output_path);
        if (!out) {
            std::cerr << "Failed to open output file: " << output_path << "\n";
            return;
        }

        out << "# Multi-hex biodiversity time series (pseudo-NetCDF)\n";
        out << "# dimensions: hex=" << num_hex_ << ", time=" << static_cast<int>(years / dt_years + 1) << "\n";
        out << std::fixed << std::setprecision(4);

        for (const auto& cell : hex_cells_) {
            auto series = sim.simulate(cell, years, dt_years);
            out << "hex_id=" << cell.hex_id
                << ", urban_intensity=" << cell.urban_intensity
                << ", initial_canopy=" << cell.initial_canopy_fraction
                << ", initial_biodiversity=" << cell.initial_biodiversity_index
                << "\n";
            out << "time_years,biodiversity_index\n";
            for (const auto& pt : series) {
                out << pt.time_years << "," << pt.biodiversity_index << "\n";
            }
            out << "----\n";
        }

        std::cout << "Habitat regeneration simulation written to " << output_path << "\n";
    }

private:
    int num_hex_;
    std::mt19937 rng_;
    std::vector<HexCell> hex_cells_;

    void generate_hex_cells() {
        std::uniform_real_distribution<double> canopy_dist(0.05, 0.4);
        std::uniform_real_distribution<double> biodiv_dist(0.2, 0.6);
        std::uniform_real_distribution<double> urban_dist(0.3, 0.9);

        hex_cells_.clear();
        for (int i = 0; i < num_hex_; ++i) {
            HexCell c;
            c.hex_id = "PHX-HX-" + std::to_string(i / 10) + "-" + std::to_string(i % 10);
            c.initial_canopy_fraction = canopy_dist(rng_);
            c.initial_biodiversity_index = biodiv_dist(rng_);
            c.urban_intensity = urban_dist(rng_);
            hex_cells_.push_back(c);
        }
    }
};

} // namespace eco

int main(int argc, char** argv) {
    using namespace eco;

    std::string out_path = "biodiversity_timeseries.txt";
    if (argc > 1) {
        out_path = argv[1];
    }

    MultiHexHabitatPlanner planner(100); // simulate 100 hex cells
    planner.run_and_write(out_path, 10.0, 0.5); // 10 years, 0.5-year steps

    return 0;
}
