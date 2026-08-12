// File: cpp/tools/prometheus_research_object_registry.cpp

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

namespace prometheus_praxis {

struct ResearchObject {
    std::string_view id;
    std::string_view path;
    std::string_view objective;
    std::string_view deliverable;
    int priority;
};

constexpr std::array<ResearchObject, 32> objects{{
    {"cpp-build-contract", "cpp/tools", "Define target dependencies and C++20 compile contracts", "CMake target matrix", 1},
    {"cpp-fixture-contract", "cpp/simulation/testdata", "Create deterministic model input fixtures", "CSV and SQLite fixtures", 1},
    {"ker-parity-tests", "cpp/eco_restoration", "Verify K E R calculations across all C++ engines", "Shared parity test suite", 1},
    {"decision-schema", "cpp/eco_restoration", "Unify executable JSON decision fields", "Versioned decision schema", 1},
    {"error-taxonomy", "cpp/tools", "Standardize validation and runtime error categories", "Machine-readable error codes", 1},
    {"sqlite-raii", "cpp/tools", "Centralize SQLite ownership statements and transactions", "C++20 SQLite utility library", 1},
    {"csv-reader", "cpp/tools", "Provide strict typed CSV parsing with row diagnostics", "Reusable CSV reader", 1},
    {"config-loader", "cpp/tools", "Load bounded model configuration without implicit defaults", "Validated configuration loader", 1},
    {"model-provenance", "cpp/tools", "Record input paths parameters and model versions", "Run manifest JSON writer", 2},
    {"unit-normalization", "cpp/eco_restoration", "Make physical units explicit before risk scoring", "Typed unit conversion module", 1},
    {"hex-anchor-contract", "cpp/eco_restoration", "Document and enforce canonical anchor encoding limits", "Anchor encode decode tests", 1},
    {"hex-correction-store", "cpp/simulation", "Load geodesic correction coefficients before placement", "SQLite coefficient reader", 2},
    {"gps-consistency-study", "cpp/simulation", "Compare anchor stability under positional uncertainty", "Monte Carlo report CSV", 2},
    {"canal-state-estimation", "cpp/simulation", "Estimate flow head and sediment state confidence", "State estimation evaluator", 2},
    {"lyapunov-validation", "cpp/simulation", "Validate basin membership over historical observations", "Basin validation report", 1},
    {"delayed-corridor-sweep", "cpp/simulation", "Sweep delay and Lipschitz bounds for safe margins", "Delay feasibility table", 2},
    {"water-model-calibration", "cpp/simulation", "Measure held-out outcome calibration by restoration site", "Calibration metrics table", 1},
    {"carbon-forecast-backtest", "cpp/simulation", "Compare carbon forecast intervals with observations", "Hourly backtest report", 2},
    {"battery-throughput-audit", "cpp/simulation", "Quantify storage use degradation and benefit tradeoffs", "Battery schedule audit", 2},
    {"renewable-scenario-audit", "cpp/simulation", "Validate scenario coverage and chance-budget outcomes", "Scenario constraint report", 2},
    {"milp-sparsity-export", "cpp/tools", "Export scheduling matrices in sparse coordinate form", "Solver-neutral matrix files", 2},
    {"milp-solution-checker", "cpp/tools", "Independently validate candidate scheduling assignments", "Feasibility checker CLI", 1},
    {"pareto-front-audit", "cpp/simulation", "Verify rank zero candidates and knee-point selection", "Pareto audit table", 2},
    {"threshold-schedule-checker", "cpp/tools", "Check time and flow threshold ranges before use", "Schedule validation CLI", 1},
    {"lua-interface-contract", "lua/eco_restoration", "Test generated Lua module exports and argument order", "Lua interface checks", 2},
    {"sql-schema-contract", "sql/eco_restoration", "Validate tables indexes constraints and query parameters", "SQLite schema smoke suite", 1},
    {"append-only-run-log", "sql/eco_restoration", "Persist model run manifests and decisions", "Auditable run ledger", 2},
    {"cross-language-vectors", "cpp/tools", "Publish shared C++ Lua and SQL test vectors", "Portable fixture bundle", 1},
    {"cli-help-standard", "cpp/tools", "Standardize argument help exit codes and JSON output", "CLI behavior contract", 2},
    {"github-example-workflows", "cpp/tools", "Provide local reproducible command examples", "Contributor command guide", 2},
    {"artifact-retention-policy", "sql/eco_restoration", "Classify fixtures models and calibration outputs", "Retention metadata schema", 3},
    {"research-report-index", "cpp/tools", "Index generated reports by model corridor and date", "Report catalog CLI", 3}
}};

void print_json(const ResearchObject& object) {
    std::cout << "{\"id\":\"" << object.id
              << "\",\"path\":\"" << object.path
              << "\",\"objective\":\"" << object.objective
              << "\",\"deliverable\":\"" << object.deliverable
              << "\",\"priority\":" << object.priority << "}";
}

}  // namespace prometheus_praxis

int main(int argc, char** argv) {
    using namespace prometheus_praxis;

    const int maximum_priority = argc == 2 ? std::clamp(std::stoi(argv[1]), 1, 3) : 3;
    std::cout << "[";

    bool first = true;
    for (const ResearchObject& object : objects) {
        if (object.priority > maximum_priority) continue;
        if (!first) std::cout << ",";
        print_json(object);
        first = false;
    }

    std::cout << "]\n";
}
