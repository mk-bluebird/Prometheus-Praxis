#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace ppx::waste::conveyance::graph {

struct SegmentTelemetry {
    std::string segment_id;
    std::string node_id;
    std::string region;
    std::string lane;

    double belt_speed_m_per_s;
    double load_kg;
    double occupancy_fraction;
    double energy_kw;
    double temperature_c;
};

struct BlastRadiusEntry {
    std::string source_type;
    std::string source_id;
    std::string target_type;
    std::string target_id;
    std::string impact_type;

    double impact_score;
    double vt_sensitivity;

    std::string notes;
};

struct WorkloadTrendEntry {
    std::string node_id;
    std::string channel;

    double total_requests_j;
    double total_surplus_j;

    double mean_vt_before;
    double mean_vt_after;

    double mean_r_carbon;
    double mean_r_biodiv;
};

struct ConveyorSegment {
    SegmentTelemetry telemetry;
    std::vector<BlastRadiusEntry> impacts;
    std::vector<WorkloadTrendEntry> workload_trends;

    bool carbon_negative_ok;
    bool restoration_ok;
    bool blastsafe_ok;
};

struct ConveyorGraph {
    std::string graph_id;
    std::vector<ConveyorSegment> segments;
    std::unordered_map<std::string, std::vector<std::string>> neighbors;
};

class EcoNetDiagnosticsClient {
public:
    explicit EcoNetDiagnosticsClient(const std::string& shared_lib_path);

    std::vector<BlastRadiusEntry> get_blast_radius_for_node(const std::string& db_path,
                                                            const std::string& node_id) const;

    std::vector<WorkloadTrendEntry> get_workload_trends_for_node(const std::string& db_path,
                                                                 const std::string& node_id) const;

private:
    std::string lib_path_;
};

class ConveyorGraphBuilder {
public:
    explicit ConveyorGraphBuilder(const std::string& econet_lib_path);

    ConveyorGraph build(const std::string& db_path,
                        const std::string& graph_id,
                        const std::vector<SegmentTelemetry>& telemetry_list,
                        const std::unordered_map<std::string, std::vector<std::string>>& neighbors) const;

private:
    EcoNetDiagnosticsClient diagnostics_client_;
};

} // namespace ppx::waste::conveyance::graph
