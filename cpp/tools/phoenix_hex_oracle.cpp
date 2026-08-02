// File: cpp/tools/phoenix_hex_oracle.cpp
#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>

// Phoenix hex oracle:
// - Given lat/lon, return:
//   * hex anchor ID,
//   * current eco-impact composite,
//   * predicted heat-island reduction from 10% canopy increase,
//   * confidence interval.
// - Target response time: < 100 ms.
// This file implements a deterministic, in-memory oracle suitable for
// integration with a higher-level API server.

namespace eco {

struct HexRecord {
    std::string hex_id;
    double eco_composite;      // overall eco-impact score (0..1)
    double baseline_heat_island; // baseline heat-island intensity metric
    double canopy_sensitivity; // predicted reduction per 10% canopy (C or index units)
    double ci_low;
    double ci_high;
};

struct OracleResponse {
    std::string hex_id;
    double eco_composite;
    double predicted_heat_reduction;
    double ci_low;
    double ci_high;
};

class PhoenixHexOracle {
public:
    PhoenixHexOracle(double hex_size_km,
                     double lat_origin,
                     double lon_origin)
        : hex_size_km_(hex_size_km),
          lat_origin_(lat_origin),
          lon_origin_(lon_origin) {
        preload_hex_table();
    }

    OracleResponse query(double lat_deg, double lon_deg) const {
        auto t0 = std::chrono::high_resolution_clock::now();

        std::string hex_id = compute_hex_id(lat_deg, lon_deg);
        const HexRecord* rec = lookup_hex(hex_id);

        OracleResponse resp;
        if (rec) {
            resp.hex_id = rec->hex_id;
            resp.eco_composite = rec->eco_composite;
            resp.predicted_heat_reduction = rec->canopy_sensitivity;
            resp.ci_low  = rec->ci_low;
            resp.ci_high = rec->ci_high;
        } else {
            resp.hex_id = "UNKNOWN";
            resp.eco_composite = 0.0;
            resp.predicted_heat_reduction = 0.0;
            resp.ci_low  = 0.0;
            resp.ci_high = 0.0;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        std::cout << "PhoenixHexOracle query latency: " << dt << " microseconds.\n";

        return resp;
    }

private:
    double hex_size_km_;
    double lat_origin_;
    double lon_origin_;
    std::vector<HexRecord> hex_table_;

    // Simple equirectangular projection to approximate hex index.
    static void project(double lat_deg, double lon_deg,
                        double lat_origin, double lon_origin,
                        double& x_km, double& y_km) {
        // Earth radius ~6371 km.
        const double R = 6371.0;
        double lat_rad = lat_deg * M_PI / 180.0;
        double lon_rad = lon_deg * M_PI / 180.0;
        double lat0_rad = lat_origin * M_PI / 180.0;
        double lon0_rad = lon_origin * M_PI / 180.0;
        double dx = (lon_rad - lon0_rad) * std::cos(lat0_rad);
        double dy = (lat_rad - lat0_rad);
        x_km = R * dx;
        y_km = R * dy;
    }

    std::string compute_hex_id(double lat_deg, double lon_deg) const {
        double x_km, y_km;
        project(lat_deg, lon_deg, lat_origin_, lon_origin_, x_km, y_km);

        // Convert (x,y) into axial coordinates for a hex grid.
        double q = (std::sqrt(3.0) / 3.0 * x_km - 1.0 / 3.0 * y_km) / hex_size_km_;
        double r = (2.0 / 3.0 * y_km) / hex_size_km_;

        int qi = static_cast<int>(std::round(q));
        int ri = static_cast<int>(std::round(r));

        return "PHX-HX-" + std::to_string(qi) + "-" + std::to_string(ri);
    }

    void preload_hex_table() {
        // In a real system, this would load from a data lake or ALN shard.
        // Here we define a few sample records.
        hex_table_.push_back(HexRecord{
            "PHX-HX-0-0",
            0.78,   // eco_composite
            1.5,    // baseline heat-island intensity
            0.4,    // predicted reduction (C) for 10% canopy
            0.3,    // ci_low
            0.5     // ci_high
        });
        hex_table_.push_back(HexRecord{
            "PHX-HX-1-0",
            0.62,
            2.1,
            0.6,
            0.4,
            0.8
        });
        hex_table_.push_back(HexRecord{
            "PHX-HX-0-1",
            0.55,
            2.3,
            0.7,
            0.5,
            0.9
        });
    }

    const HexRecord* lookup_hex(const std::string& hex_id) const {
        for (const auto& rec : hex_table_) {
            if (rec.hex_id == hex_id) {
                return &rec;
            }
        }
        return nullptr;
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Phoenix approximate origin.
    double lat_origin = 33.4484;
    double lon_origin = -112.0740;
    double hex_size_km = 1.0;

    PhoenixHexOracle oracle(hex_size_km, lat_origin, lon_origin);

    double lat = 33.4484;
    double lon = -112.0740;
    auto resp = oracle.query(lat, lon);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Hex oracle response:\n";
    std::cout << "  hex_id: " << resp.hex_id << "\n";
    std::cout << "  eco_composite: " << resp.eco_composite << "\n";
    std::cout << "  predicted_heat_reduction_10pc_canopy: "
              << resp.predicted_heat_reduction << " C\n";
    std::cout << "  confidence: [" << resp.ci_low << ", " << resp.ci_high << "]\n";

    return 0;
}
