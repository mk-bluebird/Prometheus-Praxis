// File: cpp/simulation/waste_stream_optimizer.cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

namespace eco {

struct WasteItem {
    std::string name;
    double mass_kg;
    double recyclable_fraction;
    double compostable_fraction;
    double hazardous_fraction;
};

struct WasteRoutingDecision {
    double to_recycling_kg;
    double to_compost_kg;
    double to_safe_landfill_kg;
};

class WasteStreamOptimizer {
public:
    WasteRoutingDecision route(const WasteItem &item) const {
        WasteRoutingDecision d{};
        double recyclable = item.mass_kg * item.recyclable_fraction;
        double compostable = item.mass_kg * item.compostable_fraction;
        double hazardous = item.mass_kg * item.hazardous_fraction;
        double remaining = item.mass_kg - recyclable - compostable - hazardous;
        if (remaining < 0.0) remaining = 0.0;

        d.to_recycling_kg = recyclable;
        d.to_compost_kg = compostable;
        d.to_safe_landfill_kg = remaining + hazardous;
        return d;
    }
};

} // namespace eco

int main() {
    eco::WasteStreamOptimizer opt;
    eco::WasteItem item{"Mixed household", 10.0, 0.3, 0.4, 0.05};
    auto d = opt.route(item);
    std::cout << "Routing for " << item.name << "\n";
    std::cout << "  Recycling: " << d.to_recycling_kg << " kg\n";
    std::cout << "  Compost: " << d.to_compost_kg << " kg\n";
    std::cout << "  Safe landfill: " << d.to_safe_landfill_kg << " kg\n";
    return 0;
}
