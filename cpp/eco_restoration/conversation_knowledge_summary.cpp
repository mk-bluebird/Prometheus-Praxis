// File: cpp/eco_restoration/conversation_knowledge_summary.cpp
#include <array>
#include <string_view>

namespace eco_restoration {

struct KnowledgeItem {
    std::string_view domain;
    std::string_view finding;
};

constexpr std::array<KnowledgeItem, 16> kConversationKnowledge{{
    {"Architecture", "Prometheus-Praxis uses C++20 core, simulation, and tool modules with SQLite as the evidence store."},
    {"Safety", "All analytics are advisory and non-actuating; decisions must remain reviewable by community operators."},
    {"Scoring", "Knowledge, eco-impact, and risk are normalized to [0,1], with eco-impact modeled as knowledge times restoration value times one minus risk."},
    {"Spatial", "Hex anchors use a safe packed layout of 4 level bits, 30 row bits, and 30 column bits."},
    {"Spatial", "UTM zone 12N conversion and persisted polynomial calibration coefficients support corrected local hex coordinates."},
    {"Telemetry", "Telemetry quality flags GOOD, SUSPECT, and BAD lower effective knowledge and raise conservative risk treatment."},
    {"Uncertainty", "Monte Carlo propagation, Sobol analysis, and confidence intervals should accompany lane and restoration decisions."},
    {"Water", "Canal models cover seepage fusion, water-quality CUSUM events, ET, water budgets, GDE risk, and conservative inundation diagnostics."},
    {"Ecology", "Implemented ecological coordinates include heat, biodiversity, soil carbon, canopy, bioacoustics, drought, flood, and community benefit."},
    {"RemoteSensing", "GDAL workflows aggregate drone thermal imagery, Sentinel-derived ET, DEM terrain, NDVI, and renewable-resource potential by hex."},
    {"Planning", "MILP formulations cover restoration actions, dependencies, crew and budget constraints, equity allocation, battery scheduling, and thermal-aware placement."},
    {"Forecasting", "Carbon intensity uses a persisted GARCH model; ecological trends use difference-based autoregressive forecasting with uncertainty."},
    {"Interfaces", "Local C++ services provide citizen telemetry ingestion, dashboards, carbon accounting, geodesic correction, and bounded telemetry bridges."},
    {"Governance", "Hex action conflicts are retained as version-vector evidence and presented for manual operator selection rather than silently selected."},
    {"Validation", "Historical reanalysis metrics include NSE, KGE, and RMSE; SPC monitors seven-day K/E/R process drift."},
    {"Operations", "CMake, CTest, format, Lua, SQL validation, Docker artifacts, commit validation, generated reports, and documentation support reproducible operation."},
}};

constexpr std::string_view kIntegrationGuidance =
    "Use strict SQLite schemas and prepared statements; calibrate every physical coefficient with site data; "
    "retain source provenance, uncertainty, and operator-review records with every material decision.";

}  // namespace eco_restoration
