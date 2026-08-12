// File: cpp/tools/hex_anchor_obj_export.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct HexRiskSurface {
    std::uint64_t anchor{};
    double center_x_m{};
    double center_y_m{};
    double edge_m{};
    double risk{};
};

void export_hex_risk_obj(const std::string& output_path,
                         const std::vector<HexRiskSurface>& hexes,
                         double elevation_scale_m = 10.0) {
    if (elevation_scale_m < 0.0) throw std::invalid_argument("negative elevation scale");
    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("cannot create OBJ output");

    output << "# Hex risk surface: z=risk*elevation_scale_m\n";
    std::size_t vertex_base = 1;
    constexpr double pi = 3.14159265358979323846;
    for (const auto& hex : hexes) {
        if (hex.edge_m <= 0.0) throw std::invalid_argument("non-positive hex edge");
        const double z = std::clamp(hex.risk, 0.0, 1.0) * elevation_scale_m;
        output << "o hex_" << hex.anchor << "\n";
        output << "v " << hex.center_x_m << ' ' << hex.center_y_m << ' ' << z << "\n";
        for (int corner = 0; corner < 6; ++corner) {
            const double angle = pi / 6.0 + corner * pi / 3.0;
            output << "v " << hex.center_x_m + hex.edge_m * std::cos(angle) << ' '
                   << hex.center_y_m + hex.edge_m * std::sin(angle) << ' ' << z << "\n";
        }
        for (std::size_t corner = 0; corner < 6; ++corner) {
            const std::size_t next = (corner + 1) % 6;
            output << "f " << vertex_base << ' ' << vertex_base + corner + 1 << ' '
                   << vertex_base + next + 1 << "\n";
        }
        vertex_base += 7;
    }
}

}  // namespace eco_restoration
