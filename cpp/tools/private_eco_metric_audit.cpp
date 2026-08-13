// File: cpp/tools/private_eco_metric_audit.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/private_eco_metric_comparison.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const MetricThreshold risk_threshold{
            EcoMetric::RiskOfHarm,
            Comparison::LessOrEqual,
            0.30,
            0.02,
            "unit_interval",
            "daily_restoration_window",
            "eco_metric_schema_v1"
        };
        const MetricThreshold corridor_threshold{
            EcoMetric::CorridorQuality,
            Comparison::GreaterOrEqual,
            0.70,
            0.03,
            "unit_interval",
            "weekly_corridor_window",
            "eco_metric_schema_v1"
        };
        const MetricThreshold water_threshold{
            EcoMetric::WaterUse,
            Comparison::LessOrEqual,
            35.0,
            2.0,
            "liters_per_m2_day",
            "daily_water_window",
            "eco_metric_schema_v1"
        };

        const BooleanAuditRecord risk_record =
            compare_private_eco_metric(0.24, risk_threshold, 0.97);
        const BooleanAuditRecord corridor_record =
            compare_private_eco_metric(0.78, corridor_threshold, 0.95);
        const BooleanAuditRecord water_record =
            compare_private_eco_metric(31.0, water_threshold, 0.94);

        const std::vector<BooleanAuditRecord> required_records{
            risk_record,
            corridor_record,
            water_record
        };
        const bool deployable = all_required_private_predicates_pass(required_records);

        double mean_knowledge = 0.0;
        double mean_impact = 0.0;
        for (const auto& record : required_records) {
            mean_knowledge += record.knowledge_factor;
            mean_impact += record.eco_impact_value;
        }
        mean_knowledge /= static_cast<double>(required_records.size());
        mean_impact /= static_cast<double>(required_records.size());

        std::cout << std::fixed << std::setprecision(6)
                  << "risk_predicate_passed=" << (risk_record.passed ? 1 : 0) << '\n'
                  << "corridor_predicate_passed=" << (corridor_record.passed ? 1 : 0) << '\n'
                  << "water_predicate_passed=" << (water_record.passed ? 1 : 0) << '\n'
                  << "private_metric_deployable=" << (deployable ? 1 : 0) << '\n'
                  << "knowledge_factor=" << mean_knowledge << '\n'
                  << "eco_impact_value=" << mean_impact << '\n';

        return deployable ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "private eco-metric audit failed: " << error.what() << '\n';
        return 1;
    }
}
