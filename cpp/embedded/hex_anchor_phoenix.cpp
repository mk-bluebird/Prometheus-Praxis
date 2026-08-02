// File: cpp/embedded/hex_anchor_phoenix.cpp
#include <iostream>
#include <cmath>
#include <string>

// Minimal, self-contained H3-like hex anchor computation for Phoenix edge devices.
// This does NOT implement the full Uber H3 library (which is external), but provides
// a deterministic hex anchor ID scheme suitable for embedded C++:
//  - Convert GPS lat/lon to local x,y using an equirectangular projection.
//  - Map x,y to axial hex coordinates (q,r) with a configured hex size.
//  - Format a string hex anchor ID: "PHX-H3-q-r".
//
// This can be compiled for embedded targets and run on each sensor node to
// self-assign to a Phoenix anchor cell without external dependencies.

namespace eco {

class HexAnchorPhoenix {
public:
    HexAnchorPhoenix(double hex_size_km,
                     double lat_origin_deg,
                     double lon_origin_deg)
        : hex_size_km_(hex_size_km),
          lat_origin_deg_(lat_origin_deg),
          lon_origin_deg_(lon_origin_deg) {}

    // Compute hex anchor ID from GPS coordinates (degrees).
    std::string compute_hex_anchor(double lat_deg, double lon_deg) const {
        double x_km = 0.0;
        double y_km = 0.0;
        project_to_local(lat_deg, lon_deg, x_km, y_km);

        // Axial coordinates for hex grid.
        double q = (std::sqrt(3.0) / 3.0 * x_km - 1.0 / 3.0 * y_km) / hex_size_km_;
        double r = (2.0 / 3.0 * y_km) / hex_size_km_;

        int qi = static_cast<int>(std::round(q));
        int ri = static_cast<int>(std::round(r));

        return format_hex_id(qi, ri);
    }

private:
    double hex_size_km_;
    double lat_origin_deg_;
    double lon_origin_deg_;

    // Equirectangular projection, sufficient for local Phoenix use.
    static void project_to_local(double lat_deg, double lon_deg,
                                 double& x_km, double& y_km,
                                 double lat_origin_deg = 33.4484,
                                 double lon_origin_deg = -112.0740) {
        const double R = 6371.0; // Earth radius in km.
        double lat_rad = lat_deg * M_PI / 180.0;
        double lon_rad = lon_deg * M_PI / 180.0;
        double lat0_rad = lat_origin_deg * M_PI / 180.0;
        double lon0_rad = lon_origin_deg * M_PI / 180.0;

        double dx = (lon_rad - lon0_rad) * std::cos(lat0_rad);
        double dy = (lat_rad - lat0_rad);

        x_km = R * dx;
        y_km = R * dy;
    }

    std::string format_hex_id(int q, int r) const {
        return "PHX-H3-" + std::to_string(q) + "-" + std::to_string(r);
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Phoenix origin (approximate downtown) and 1 km hex size.
    HexAnchorPhoenix anchor(1.0, 33.4484, -112.0740);

    // Example sensor GPS coordinates.
    double lat_sensor = 33.4500;
    double lon_sensor = -112.0700;

    std::string hex_id = anchor.compute_hex_anchor(lat_sensor, lon_sensor);
    std::cout << "Hex anchor ID for sensor (" << lat_sensor << ", " << lon_sensor
              << ") is: " << hex_id << "\n";

    return 0;
}
