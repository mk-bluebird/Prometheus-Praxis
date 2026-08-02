// File: cpp/tools/eco_impact_reporter.cpp
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <iomanip>

namespace eco {

struct SensorReading {
    std::string sensor_id;
    std::string quantity;
    double value;
    std::string unit;
    std::chrono::system_clock::time_point timestamp;
};

struct EcoImpactRecord {
    std::string namespace_id;
    std::string module_id;
    std::string corridor_id;
    std::string impact_metric;
    double impact_value;
    double confidence_low;
    double confidence_high;
    std::vector<SensorReading> provenance;
};

struct EcoImpactEntry {
    std::string entity_name;
    double knowledge_factor;
    double eco_impact_value;
};

class EcoImpactReporter {
public:
    explicit EcoImpactReporter(const std::string& ns)
        : namespace_id_(ns) {}

    EcoImpactRecord make_record(const std::string& module_id,
                                const std::string& corridor_id,
                                const std::string& impact_metric,
                                double impact_value,
                                double confidence_low,
                                double confidence_high,
                                const std::vector<SensorReading>& provenance) const {
        EcoImpactRecord rec;
        rec.namespace_id   = namespace_id_;
        rec.module_id      = module_id;
        rec.corridor_id    = corridor_id;
        rec.impact_metric  = impact_metric;
        rec.impact_value   = impact_value;
        rec.confidence_low = confidence_low;
        rec.confidence_high= confidence_high;
        rec.provenance     = provenance;
        return rec;
    }

    void add_entry(const EcoImpactEntry& entry) {
        entries_.push_back(entry);
    }

    void add_record(const EcoImpactRecord& record) {
        records_.push_back(record);
    }

    void print_entry_report() const {
        std::cout << "Eco impact entry report\n";
        std::cout << std::left << std::setw(25) << "Entity"
                  << std::setw(15) << "Knowledge"
                  << std::setw(15) << "Eco impact" << "\n";
        for (const auto& e : entries_) {
            std::cout << std::left << std::setw(25) << e.entity_name
                      << std::setw(15) << std::fixed << std::setprecision(3) << e.knowledge_factor
                      << std::setw(15) << std::fixed << std::setprecision(3) << e.eco_impact_value
                      << "\n";
        }
    }

    static void print_record(const EcoImpactRecord& rec) {
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "EcoImpactRecord {\n";
        std::cout << "  namespace_id: " << rec.namespace_id << "\n";
        std::cout << "  module_id:    " << rec.module_id << "\n";
        std::cout << "  corridor_id:  " << rec.corridor_id << "\n";
        std::cout << "  impact_metric:" << rec.impact_metric << "\n";
        std::cout << "  impact_value: " << rec.impact_value << "\n";
        std::cout << "  confidence:   [" << rec.confidence_low
                  << ", " << rec.confidence_high << "]\n";
        std::cout << "  provenance:   [\n";
        for (const auto& s : rec.provenance) {
            std::time_t tt = std::chrono::system_clock::to_time_t(s.timestamp);
            std::cout << "    {sensor_id: " << s.sensor_id
                      << ", quantity: " << s.quantity
                      << ", value: " << s.value
                      << " " << s.unit
                      << ", timestamp: " << std::put_time(std::gmtime(&tt), "%F %T")
                      << "}\n";
        }
        std::cout << "  ]\n";
        std::cout << "}\n";
    }

private:
    std::string namespace_id_;
    std::vector<EcoImpactEntry> entries_;
    std::vector<EcoImpactRecord> records_;
};

class CompostImpactModule {
public:
    CompostImpactModule(const EcoImpactReporter& reporter,
                        const std::string& corridor_id)
        : reporter_(reporter), corridor_id_(corridor_id) {}

    EcoImpactRecord run(double eco_score,
                        double ci_low,
                        double ci_high,
                        const std::vector<SensorReading>& sensors) const {
        return reporter_.make_record("compost_pile_simulator",
                                     corridor_id_,
                                     "compost_eco_score",
                                     eco_score,
                                     ci_low,
                                     ci_high,
                                     sensors);
    }

private:
    const EcoImpactReporter& reporter_;
    std::string corridor_id_;
};

} // namespace eco

int main() {
    using namespace eco;

    EcoImpactReporter reporter("Prometheus-Praxis/Phoenix");

    reporter.add_entry(EcoImpactEntry{"Bamboo decking", 0.9, 0.82});
    reporter.add_entry(EcoImpactEntry{"Recycled steel frame", 0.95, 0.65});
    reporter.print_entry_report();

    std::vector<SensorReading> sensors;
    sensors.push_back(SensorReading{
        "sensor-soil-001",
        "soil_moisture",
        38.5,
        "%",
        std::chrono::system_clock::now()
    });
    sensors.push_back(SensorReading{
        "sensor-temp-017",
        "air_temp",
        39.2,
        "C",
        std::chrono::system_clock::now()
    });

    CompostImpactModule compost_module(reporter, "PHX-HEX-005");
    EcoImpactRecord rec = compost_module.run(0.82, 0.75, 0.88, sensors);
    EcoImpactReporter::print_record(rec);

    return 0;
}
