// File: cpp/eco_restoration/smart_waste_bin_logic.cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace eco {

struct HexWasteStatus {
    std::string hex_id;
    double bin_fill_level;    // 0..1
    double lyapunov_residual; // V_t^{(h)}, lower is better for eco-stability
    double carbon_intensity;  // current forecast CI
    double max_carbon;        // corridor max
};

struct TruckRouteStop {
    std::string hex_id;
    double priority_score;
};

double carbon_corridor(double ci, double max_carbon, double c_min) {
    if (max_carbon <= 0.0) return 0.0;
    double c = 1.0 - ci / max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    if (c < c_min) c = c_min;
    return c;
}

// Compute route priority: bins with high fill, low Lyapunov residual (safe capacity),
// and strong carbon corridor (low CI) get routed first.
double compute_priority(const HexWasteStatus& h, double c_min) {
    double c = carbon_corridor(h.carbon_intensity, h.max_carbon, c_min);
    double fill_weight = h.bin_fill_level;            // more waste -> higher priority
    double lyap_weight = std::exp(-h.lyapunov_residual); // lower V -> larger exp
    double carbon_weight = c;                         // stronger corridor -> higher

    return fill_weight * lyap_weight * carbon_weight;
}

// Generate truck route based on real-time waste, Lyapunov, and carbon forecasts.
std::vector<TruckRouteStop> plan_route(const std::vector<HexWasteStatus>& hexes,
                                       double c_min,
                                       std::size_t max_stops) {
    std::vector<TruckRouteStop> stops;
    for (const auto& h : hexes) {
        double score = compute_priority(h, c_min);
        stops.push_back({h.hex_id, score});
    }

    std::sort(stops.begin(), stops.end(),
              [](const TruckRouteStop& a, const TruckRouteStop& b) {
                  return a.priority_score > b.priority_score;
              });

    if (stops.size() > max_stops) {
        stops.resize(max_stops);
    }
    return stops;
}

void print_route_sql(const std::vector<TruckRouteStop>& stops,
                     const std::string& route_id) {
    std::cout << "DELETE FROM smart_waste_route WHERE route_id = '"
              << route_id << "';\n";
    std::cout << "INSERT INTO smart_waste_route (route_id, stop_order, hex_id, priority_score) VALUES\n";
    for (std::size_t i = 0; i < stops.size(); ++i) {
        std::cout << "('" << route_id << "', " << i << ", '"
                  << stops[i].hex_id << "', "
                  << stops[i].priority_score << ")";
        if (i + 1 < stops.size()) {
            std::cout << ",\n";
        } else {
            std::cout << ";\n";
        }
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example real-time statuses: hex bins, Lyapunov residuals, carbon forecasts.
    std::vector<HexWasteStatus> hexes = {
        {"hex_W1", 0.9, 0.10, 0.3, 1.0},
        {"hex_W2", 0.7, 0.25, 0.5, 1.0},
        {"hex_W3", 0.4, 0.08, 0.2, 1.0},
        {"hex_W4", 0.6, 0.15, 0.35, 1.0}
    };

    double c_min = 0.2;
    std::size_t max_stops = 3;
    std::string route_id = "route_2026_08_03_AM";

    auto route = plan_route(hexes, c_min, max_stops);

    std::cout << "Smart waste bin route (corridor-aware):\n";
    for (std::size_t i = 0; i < route.size(); ++i) {
        std::cout << "  stop " << i << ": " << route[i].hex_id
                  << " (score=" << route[i].priority_score << ")\n";
    }
    std::cout << "\n";

    print_route_sql(route, route_id);

    return 0;
}
