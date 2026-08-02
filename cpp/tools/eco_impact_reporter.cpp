// File: cpp/tools/eco_impact_reporter.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

namespace eco {

struct EcoImpactEntry {
    std::string entity_name;
    double knowledge_factor;
    double eco_impact_value;
};

class EcoImpactReporter {
public:
    void add_entry(const EcoImpactEntry &entry) {
        entries_.push_back(entry);
    }

    void print_report() const {
        std::cout << "Eco impact report\n";
        std::cout << std::left << std::setw(25) << "Entity"
                  << std::setw(15) << "Knowledge"
                  << std::setw(15) << "Eco impact" << "\n";
        for (const auto &e : entries_) {
            std::cout << std::left << std::setw(25) << e.entity_name
                      << std::setw(15) << std::fixed << std::setprecision(3) << e.knowledge_factor
                      << std::setw(15) << std::fixed << std::setprecision(3) << e.eco_impact_value
                      << "\n";
        }
    }

private:
    std::vector<EcoImpactEntry> entries_;
};

} // namespace eco

int main() {
    eco::EcoImpactReporter reporter;
    reporter.add_entry({"Bamboo decking", 0.9, 0.82});
    reporter.add_entry({"Recycled steel frame", 0.95, 0.65});
    reporter.print_report();
    return 0;
}
