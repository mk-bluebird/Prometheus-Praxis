// aletheion_rm/materials/ALE-RM-MAT-CARBON-TRACKER-001.hpp
// Carbon and embodied-energy tracker interface for eco_restoration_shard / Prometheus-Praxis.
// Couples cyboquatic hardware, pumps, substrates, and drone sorties to lifecycle metrics. [file:17]

#ifndef ALE_RM_MAT_CARBON_TRACKER_001_HPP
#define ALE_RM_MAT_CARBON_TRACKER_001_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace aletheion {
namespace rm {
namespace materials {

struct CarbonRecord {
    std::string asset_id;       // e.g., "DRONE-WINDNET-DOWNTOWN-001"
    std::string asset_kind;     // "drone_sortie", "pump_cycle", "substrate_batch".
    double energy_kWh;          // operational energy consumed.
    double embodied_kgCO2e;     // lifecycle / embodied emissions.
    double avoided_kgCO2e;      // emissions avoided (e.g., reduced truck miles).
    double eco_score;           // normalized eco-restoration score [0,1].
    std::string corridor_id;    // corridor or neighborhood ID.
    std::uint64_t timestamp_s;  // UNIX epoch seconds.
};

struct CarbonSummary {
    double total_energy_kWh;
    double total_embodied_kgCO2e;
    double total_avoided_kgCO2e;
    double mean_eco_score;
    std::size_t record_count;
};

// CarbonTracker is a thin C++ façade over the Rust/Lua materials ledger;
// it does not make policy decisions, only records and aggregates. [file:17]
class CarbonTracker {
public:
    CarbonTracker();

    // Record a generic carbon event for any eco_restoration asset.
    void record(const CarbonRecord& rec);

    // Specialized helper for drone sorties, used by WindNet corridor services.
    void record_drone_sortie(const std::string& sortie_id,
                             double battery_kWh,
                             double expected_trash_kg,
                             const std::string& corridor_id);

    // Query aggregated metrics for reporting, stability proofs, and KER scoring.
    CarbonSummary summary_for_corridor(const std::string& corridor_id) const;
    CarbonSummary summary_for_asset_kind(const std::string& asset_kind) const;
    CarbonSummary summary_global() const;

private:
    // Internal bridge to Rust/Lua implementations.
    void record_internal(const CarbonRecord& rec);
    CarbonSummary summary_for_corridor_internal(const std::string& corridor_id) const;
    CarbonSummary summary_for_asset_kind_internal(const std::string& asset_kind) const;
    CarbonSummary summary_global_internal() const;
};

} // namespace materials
} // namespace rm
} // namespace aletheion

#endif // ALE_RM_MAT_CARBON_TRACKER_001_HPP
