// File: cpp/simulation/community_recycling_scheduler.cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

namespace eco {

struct PickupRequest {
    std::string neighborhood;
    double expected_volume_m3;
    int priority; // lower is higher priority
};

struct ScheduleEntry {
    std::string neighborhood;
    int day_offset;
};

class CommunityRecyclingScheduler {
public:
    std::vector<ScheduleEntry> build_schedule(const std::vector<PickupRequest> &requests, int days_available) const {
        std::vector<PickupRequest> sorted = requests;
        std::sort(sorted.begin(), sorted.end(),
                  [](const PickupRequest &a, const PickupRequest &b) {
                      return a.priority < b.priority;
                  });
        std::vector<ScheduleEntry> schedule;
        int day = 0;
        for (const auto &req : sorted) {
            schedule.push_back({req.neighborhood, day});
            day = (day + 1) % days_available;
        }
        return schedule;
    }
};

} // namespace eco

int main() {
    std::vector<eco::PickupRequest> reqs{
        {"District A", 5.0, 1},
        {"District B", 3.0, 2},
        {"District C", 4.0, 0}
    };
    eco::CommunityRecyclingScheduler scheduler;
    auto schedule = scheduler.build_schedule(reqs, 3);
    for (const auto &e : schedule) {
        std::cout << "Neighborhood " << e.neighborhood
                  << " scheduled in " << e.day_offset << " day(s)\n";
    }
    return 0;
}
