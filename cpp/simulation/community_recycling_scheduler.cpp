// File: cpp/simulation/community_recycling_scheduler.cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <iomanip>

namespace eco {

struct PickupRequest {
    std::string neighborhood;
    double expected_volume_m3;
    int priority;
};

struct ScheduleEntry {
    std::string neighborhood;
    int day_offset;
};

struct RecyclingRoute {
    std::string route_id;
    std::string neighborhood;
    double mass_recycled_kg;
    double distance_to_facility_km;
    double fuel_used_liters;
    double vehicle_capacity_kg;
};

double compute_route_efficiency(const RecyclingRoute& r) {
    if (r.fuel_used_liters <= 0.0 || r.vehicle_capacity_kg <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    double numerator   = r.mass_recycled_kg * r.distance_to_facility_km;
    double denominator = r.fuel_used_liters * r.vehicle_capacity_kg;
    return numerator / denominator;
}

class CommunityRecyclingScheduler {
public:
    void add_pickup_request(const PickupRequest& req) {
        pickup_requests_.push_back(req);
    }

    void add_route(const RecyclingRoute& route) {
        routes_.push_back(route);
    }

    std::vector<ScheduleEntry> build_schedule(int days_available) const {
        std::vector<PickupRequest> sorted = pickup_requests_;
        std::sort(sorted.begin(), sorted.end(),
                  [](const PickupRequest& a, const PickupRequest& b) {
                      return a.priority < b.priority;
                  });
        std::vector<ScheduleEntry> schedule;
        int day = 0;
        for (const auto& req : sorted) {
            schedule.push_back(ScheduleEntry{req.neighborhood, day});
            day = (day + 1) % days_available;
        }
        return schedule;
    }

    RecyclingRoute select_best_route_for_neighborhood(const std::string& neighborhood) const {
        double best_cost = std::numeric_limits<double>::infinity();
        const RecyclingRoute* best = nullptr;
        for (const auto& r : routes_) {
            if (r.neighborhood != neighborhood) continue;
            double eta = compute_route_efficiency(r);
            if (eta < best_cost) {
                best_cost = eta;
                best = &r;
            }
        }
        if (!best) {
            return RecyclingRoute{"NONE", neighborhood, 0.0, 0.0, 0.0, 0.0};
        }
        return *best;
    }

    void print_routes_with_costs() const {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Community recycling routes:\n";
        for (const auto& r : routes_) {
            double eta = compute_route_efficiency(r);
            std::cout << "  Route " << r.route_id
                      << " neighborhood=" << r.neighborhood
                      << " mass=" << r.mass_recycled_kg << " kg"
                      << " dist=" << r.distance_to_facility_km << " km"
                      << " fuel=" << r.fuel_used_liters << " L"
                      << " cap=" << r.vehicle_capacity_kg << " kg"
                      << " eta_route=" << eta << "\n";
        }
    }

private:
    std::vector<PickupRequest> pickup_requests_;
    std::vector<RecyclingRoute> routes_;
};

} // namespace eco

int main() {
    using namespace eco;

    CommunityRecyclingScheduler scheduler;

    scheduler.add_pickup_request(PickupRequest{"District A", 5.0, 1});
    scheduler.add_pickup_request(PickupRequest{"District B", 3.0, 2});
    scheduler.add_pickup_request(PickupRequest{"District C", 4.0, 0});

    std::vector<ScheduleEntry> schedule = scheduler.build_schedule(3);
    std::cout << "Pickup schedule:\n";
    for (const auto& e : schedule) {
        std::cout << "  Neighborhood " << e.neighborhood
                  << " scheduled in " << e.day_offset << " day(s)\n";
    }
    std::cout << "\n";

    scheduler.add_route(RecyclingRoute{"PHX-R1", "District A", 1200.0, 8.0, 45.0, 3000.0});
    scheduler.add_route(RecyclingRoute{"PHX-R2", "District B", 900.0, 12.0, 40.0, 2500.0});
    scheduler.add_route(RecyclingRoute{"PHX-R3", "District C", 1500.0, 10.0, 55.0, 3500.0});
    scheduler.add_route(RecyclingRoute{"PHX-R4", "District A", 1100.0, 7.0, 42.0, 2800.0});

    scheduler.print_routes_with_costs();

    std::cout << "\nBest routes by neighborhood (minimal eta_route):\n";
    for (const auto& e : schedule) {
        RecyclingRoute best = scheduler.select_best_route_for_neighborhood(e.neighborhood);
        std::cout << "  Neighborhood " << e.neighborhood
                  << " best_route=" << best.route_id << "\n";
    }

    return 0;
}
