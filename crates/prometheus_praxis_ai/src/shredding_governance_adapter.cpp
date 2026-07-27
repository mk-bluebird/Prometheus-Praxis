#include "ppx/waste/shredding_governance_adapter.hpp"

#include <stdexcept>
#include <cstring>

// Minimal JSON parsing stub; replace with your preferred header-only JSON library.
// For example, you can use nlohmann::json or a custom lightweight parser wired
// to your AI-chat toolchain.
#include <nlohmann/json.hpp>

extern "C" {

// Read-only EcoNet FFI: blast-radius diagnostics per node.
// Signature pattern mirrors econetgetblastradiusfornode in ecorestorationshard.
char* econetgetblastradiusfornode(const char* dbpath, const char* nodeid);

// Read-only governance FFI: lane admissibility per shard.
char* econetlanegovernancecheck(const char* dbpath, const char* shardid);

// Shared free function for JSON buffers allocated by Rust.
void econetfreejson(char* ptr);

} // extern "C"

namespace ppx::waste::shredding::governance {

using nlohmann::json;

namespace {

std::string call_ffi_json(const char* (*fn)(const char*, const char*),
                          const std::string& db_path,
                          const std::string& key_arg) {
    std::string db = db_path;
    std::string arg = key_arg;

    char* c_db = const_cast<char*>(db.c_str());
    char* c_arg = const_cast<char*>(arg.c_str());

    char* raw = fn(c_db, c_arg);
    if (raw == nullptr) {
        throw std::runtime_error("FFI returned null JSON buffer");
    }

    std::string out(raw);
    econetfreejson(raw);
    return out;
}

KerSnapshot parse_ker_snapshot_from_blastradius_json(const json& j,
                                                     const std::string& machine_id) {
    KerSnapshot snapshot;

    snapshot.machine_id = machine_id;

    // These fields are populated by the Rust blastradius kernel and governance spine.
    snapshot.region = j.value("region", "");
    snapshot.lane   = j.value("lane", "");

    snapshot.carbon_radius      = j.value("carbon_radius", 0.0);
    snapshot.biodiversity_radius = j.value("biodiversity_radius", 0.0);

    snapshot.k_score  = j.value("kscore", 0.0);
    snapshot.e_score  = j.value("escore", 0.0);
    snapshot.r_score  = j.value("rscore", 0.0);

    snapshot.vt_residual = j.value("vtresidual", 0.0);
    snapshot.roh_scalar  = j.value("rohscalar", 0.0);

    snapshot.ker_weighted_carbon_radius =
        j.value("kerweightedcarbonradius", snapshot.carbon_radius);
    snapshot.ker_weighted_biodiversity_radius =
        j.value("kerweightedbiodiversityradius", snapshot.biodiversity_radius);

    return snapshot;
}

bool parse_lane_admissible_from_json(const json& j, std::string& reason_out) {
    bool admissible = j.value("admissible", false);
    reason_out = j.value("reason", "");
    return admissible;
}

} // namespace

BlastRadiusClient::BlastRadiusClient(const std::string& shared_lib_path)
    : lib_path_(shared_lib_path) {
    // The shared_lib_path is retained for symmetry with other clients;
    // dynamic loading is assumed to be handled by the process or a higher layer.
}

KerSnapshot BlastRadiusClient::fetch_ker_snapshot(const std::string& db_path,
                                                  const std::string& machine_id) const {
    // In your Rust FFI, blast-radius kernels are generally keyed by nodeid;
    // here we treat machine_id as the node identifier for shredders.
    std::string json_str = call_ffi_json(&econetgetblastradiusfornode, db_path, machine_id);

    json j = json::parse(json_str);
    return parse_ker_snapshot_from_blastradius_json(j, machine_id);
}

LaneGovernanceClient::LaneGovernanceClient(const std::string& shared_lib_path)
    : lib_path_(shared_lib_path) {
}

bool LaneGovernanceClient::check_lane_admissible(const std::string& db_path,
                                                 const std::string& shard_id,
                                                 std::string& reason_out) const {
    std::string json_str = call_ffi_json(&econetlanegovernancecheck, db_path, shard_id);

    json j = json::parse(json_str);
    return parse_lane_admissible_from_json(j, reason_out);
}

ShreddingGovernanceAdapter::ShreddingGovernanceAdapter(const std::string& blastradius_lib_path,
                                                       const std::string& governance_lib_path)
    : blastradius_client_(blastradius_lib_path),
      lane_governance_client_(governance_lib_path) {
}

ShreddingKerSnapshot ShreddingGovernanceAdapter::compute_snapshot(
    const std::string& db_path,
    const ShredderTelemetry& shredder,
    const ScreenDrumTelemetry& screen) const {

    // Fetch KER-weighted blast-radius snapshot for the shredding machine/node.
    KerSnapshot ker = blastradius_client_.fetch_ker_snapshot(db_path, shredder.machine_id);

    // Lane governance check: shard_id is aligned with machine_id in your governance views
    // (e.g. shardinstance and vlaneadmissibility).
    std::string lane_reason;
    bool lane_ok = lane_governance_client_.check_lane_admissible(db_path,
                                                                 shredder.machine_id,
                                                                 lane_reason);

    ShreddingKerSnapshot out;
    out.shredder = shredder;
    out.screen   = screen;
    out.ker      = ker;

    // Derived flags: these are simple projections of governance metrics.
    out.carbon_negative_ok = ker.ker_weighted_carbon_radius <= ker.carbon_radius &&
                             j.value("carbonnegativeok", true);
    out.restoration_ok     = j.value("restorationok", true);
    out.lane_admissible    = lane_ok;

    return out;
}

} // namespace ppx::waste::shredding::governance
