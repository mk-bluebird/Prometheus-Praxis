// File: cpp/simulation/cyboquatic_workload_2026_08_09.cpp
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

extern "C" {

struct WorkloadInput {
    double flow_m3_s;
    double lift_m;
    double efficiency;
    double runtime_s;
    double voltage_drop_v;
    double renewable_fraction;
    double embodied_carbon_g_per_j;
    double biodiversity_risk;
};

struct WorkloadAssessment {
    double energyreq_j;
    double delta_vt;
    double knowledge_factor;
    double eco_impact_value;
    unsigned char accepted;
};

int assess_workload(const WorkloadInput* input, WorkloadAssessment* output);

}

static_assert(std::is_standard_layout_v<WorkloadInput>);
static_assert(std::is_standard_layout_v<WorkloadAssessment>);

namespace {

constexpr int kUsageError = 1;
constexpr int kRejected = 2;
constexpr int kFfiError = 3;

struct CliTelemetry {
    std::string node_id;
    WorkloadInput workload;
};

bool parse_finite_double(const char* text, double& value) {
    try {
        std::size_t consumed = 0;
        value = std::stod(text, &consumed);
        return text[consumed] == '\0' && std::isfinite(value);
    } catch (const std::exception&) {
        return false;
    }
}

bool valid_node_id(const std::string& node_id) {
    return !node_id.empty() && node_id.size() <= 128;
}

bool valid_result(const WorkloadAssessment& result) {
    return std::isfinite(result.energyreq_j) && result.energyreq_j >= 0.0 &&
           std::isfinite(result.delta_vt) && result.delta_vt >= 0.0 &&
           result.delta_vt <= 1.0 &&
           std::isfinite(result.knowledge_factor) &&
           result.knowledge_factor >= 0.0 && result.knowledge_factor <= 1.0 &&
           std::isfinite(result.eco_impact_value) &&
           result.eco_impact_value >= 0.0 && result.eco_impact_value <= 1.0 &&
           result.accepted <= 1U;
}

void print_usage(const char* program) {
    std::cerr
        << "Usage: " << program
        << " node_id flow_m3_s lift_m efficiency runtime_s voltage_drop_v"
        << " renewable_fraction embodied_carbon_g_per_j biodiversity_risk\n";
}

bool parse_arguments(int argc, char* argv[], CliTelemetry& telemetry) {
    if (argc == 1) {
        telemetry = {
            "phoenix-canal-pump-01",
            {0.035, 4.2, 0.78, 900.0, 2.1, 0.82, 0.000035, 0.08}
        };
        return true;
    }

    if (argc != 10) {
        return false;
    }

    telemetry.node_id = argv[1];
    return valid_node_id(telemetry.node_id) &&
           parse_finite_double(argv[2], telemetry.workload.flow_m3_s) &&
           parse_finite_double(argv[3], telemetry.workload.lift_m) &&
           parse_finite_double(argv[4], telemetry.workload.efficiency) &&
           parse_finite_double(argv[5], telemetry.workload.runtime_s) &&
           parse_finite_double(argv[6], telemetry.workload.voltage_drop_v) &&
           parse_finite_double(argv[7], telemetry.workload.renewable_fraction) &&
           parse_finite_double(argv[8], telemetry.workload.embodied_carbon_g_per_j) &&
           parse_finite_double(argv[9], telemetry.workload.biodiversity_risk);
}

}  // namespace

int main(int argc, char* argv[]) {
    CliTelemetry telemetry{};
    if (!parse_arguments(argc, argv, telemetry)) {
        print_usage(argv[0]);
        return kUsageError;
    }

    WorkloadAssessment result{};
    const int status = assess_workload(&telemetry.workload, &result);
    if (status != 0) {
        std::cerr << "cyboquatic-core assessment failed with status=" << status << '\n';
        return kFfiError;
    }

    if (!valid_result(result)) {
        std::cerr << "cyboquatic-core returned an invalid assessment\n";
        return kFfiError;
    }

    std::cout << std::fixed << std::setprecision(6)
              << "node_id=" << telemetry.node_id << '\n'
              << "energyreqJ=" << result.energyreq_j << '\n'
              << "deltaVt=" << result.delta_vt << '\n'
              << "knowledge_factor=" << result.knowledge_factor << '\n'
              << "eco_impact_value=" << result.eco_impact_value << '\n'
              << "accepted=" << static_cast<unsigned int>(result.accepted) << '\n';

    return result.accepted == 1U ? 0 : kRejected;
}
